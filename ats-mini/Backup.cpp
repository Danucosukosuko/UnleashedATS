#include "Common.h"
#include "Storage.h"
#include "Utils.h"
#include "Backup.h"

#include <nvs.h>
#include <nvs_flash.h>

// What a backup holds. The network namespace has the Wi-Fi passwords.
static const char *const namespaces[] =
{
  "settings", "bands", "memories", "themes", "stations", "debug", "network"
};

static const struct { nvs_type_t type; const char *name; } types[] =
{
  { NVS_TYPE_U8, "u8" }, { NVS_TYPE_I8, "i8" }, { NVS_TYPE_U16, "u16" }, { NVS_TYPE_I16, "i16" },
  { NVS_TYPE_U32, "u32" }, { NVS_TYPE_I32, "i32" }, { NVS_TYPE_U64, "u64" }, { NVS_TYPE_I64, "i64" },
  { NVS_TYPE_STR, "str" }, { NVS_TYPE_BLOB, "blob" },
};

static bool restoring = false;
static int restored = 0;
static uint8_t cleared = 0;   // Namespaces already cleared in this restore

static void writeEscaped(Print &out, const char *text)
{
  for(const uint8_t *p = (const uint8_t *)text ; *p ; p++)
    if(*p <= ' ' || *p == '%' || *p >= 0x7F) out.printf("%%%02X", *p);
    else out.print((char)*p);
}

static void writeEntry(Print &out, nvs_handle_t handle, const char *ns, const char *key, nvs_type_t type)
{
  const char *name = nullptr;
  for(size_t i=0 ; i<ITEM_COUNT(types) ; i++)
    if(types[i].type == type) name = types[i].name;
  if(!name) return;

  char number[24] = "";
  switch(type)
  {
    case NVS_TYPE_U8:  { uint8_t v;  if(nvs_get_u8(handle, key, &v)  != ESP_OK) return; sprintf(number, "%u", v); break; }
    case NVS_TYPE_I8:  { int8_t v;   if(nvs_get_i8(handle, key, &v)  != ESP_OK) return; sprintf(number, "%d", v); break; }
    case NVS_TYPE_U16: { uint16_t v; if(nvs_get_u16(handle, key, &v) != ESP_OK) return; sprintf(number, "%u", v); break; }
    case NVS_TYPE_I16: { int16_t v;  if(nvs_get_i16(handle, key, &v) != ESP_OK) return; sprintf(number, "%d", v); break; }
    case NVS_TYPE_U32: { uint32_t v; if(nvs_get_u32(handle, key, &v) != ESP_OK) return; sprintf(number, "%lu", (unsigned long)v); break; }
    case NVS_TYPE_I32: { int32_t v;  if(nvs_get_i32(handle, key, &v) != ESP_OK) return; sprintf(number, "%ld", (long)v); break; }
    case NVS_TYPE_U64: { uint64_t v; if(nvs_get_u64(handle, key, &v) != ESP_OK) return; sprintf(number, "%llu", (unsigned long long)v); break; }
    case NVS_TYPE_I64: { int64_t v;  if(nvs_get_i64(handle, key, &v) != ESP_OK) return; sprintf(number, "%lld", (long long)v); break; }
    default: break;
  }

  out.print(ns);
  out.print(' ');
  writeEscaped(out, key);
  out.print(' ');
  out.print(name);
  out.print(' ');

  if(type == NVS_TYPE_STR || type == NVS_TYPE_BLOB)
  {
    size_t size = 0;
    esp_err_t err = type == NVS_TYPE_STR? nvs_get_str(handle, key, nullptr, &size) : nvs_get_blob(handle, key, nullptr, &size);
    if(err != ESP_OK || size > 512) { out.print("\r\n"); return; }
    uint8_t data[513];
    err = type == NVS_TYPE_STR? nvs_get_str(handle, key, (char *)data, &size) : nvs_get_blob(handle, key, data, &size);
    if(err == ESP_OK && type == NVS_TYPE_STR) writeEscaped(out, (const char *)data);
    else if(err == ESP_OK) for(size_t i=0 ; i<size ; i++) out.printf("%02x", data[i]);
  }
  else out.print(number);
  out.print("\r\n");
}

void backupWrite(Print &out, bool withNetwork)
{
  out.print("# UnleashedATS backup\r\n# ");
  out.print(getVersion());
  out.print("\r\n");

  for(size_t n=0 ; n<ITEM_COUNT(namespaces) ; n++)
  {
    const char *ns = namespaces[n];
    if(!withNetwork && !strcmp(ns, "network")) continue;

    nvs_handle_t handle;
    if(nvs_open_from_partition(STORAGE_PARTITION, ns, NVS_READONLY, &handle) != ESP_OK) continue;

    nvs_iterator_t it = nullptr;
    esp_err_t err = nvs_entry_find(STORAGE_PARTITION, ns, NVS_TYPE_ANY, &it);
    while(err == ESP_OK)
    {
      nvs_entry_info_t info;
      nvs_entry_info(it, &info);
      writeEntry(out, handle, ns, info.key, info.type);
      err = nvs_entry_next(&it);
    }
    nvs_release_iterator(it);
    nvs_close(handle);
  }
  out.print("# end\r\n");
}

void backupRestoreBegin()
{
  // The settings in memory must not overwrite what is restored
  prefsFreeze();
  restoring = true;
  restored = 0;
  cleared = 0;
}

static void unescape(char *text)
{
  char *out = text;
  for(char *p = text ; *p ; p++)
  {
    if(*p == '%' && isxdigit((uint8_t)p[1]) && isxdigit((uint8_t)p[2]))
    {
      char hex[3] = { p[1], p[2], 0 };
      *out++ = (char)strtoul(hex, nullptr, 16);
      p += 2;
    }
    else *out++ = *p;
  }
  *out = '\0';
}

const char *backupRestoreLine(const char *line)
{
  if(!restoring) return("No restore in progress");
  if(!*line || *line == '#') return(nullptr);

  char copy[256];
  strlcpy(copy, line, sizeof(copy));
  char *ns = strtok(copy, " ");
  char *key = strtok(nullptr, " ");
  char *type = strtok(nullptr, " ");
  char *value = strtok(nullptr, "\r\n");
  if(!ns || !key || !type) return("Malformed line");
  if(!value) value = (char *)"";

  // Only what a backup holds
  int n = -1;
  for(size_t i=0 ; i<ITEM_COUNT(namespaces) ; i++)
    if(!strcmp(ns, namespaces[i])) n = i;
  if(n < 0) return("Unknown namespace");
  unescape(key);
  if(strlen(key) > 15) return("Key too long");

  nvs_handle_t handle;
  if(nvs_open_from_partition(STORAGE_PARTITION, ns, NVS_READWRITE, &handle) != ESP_OK) return("Unable to open the settings");

  // The backup replaces the namespace as a whole
  if(!(cleared & (1 << n)))
  {
    nvs_erase_all(handle);
    cleared |= 1 << n;
  }

  esp_err_t err = ESP_FAIL;
  if(!strcmp(type, "u8"))       err = nvs_set_u8(handle, key, strtoul(value, nullptr, 10));
  else if(!strcmp(type, "i8"))  err = nvs_set_i8(handle, key, strtol(value, nullptr, 10));
  else if(!strcmp(type, "u16")) err = nvs_set_u16(handle, key, strtoul(value, nullptr, 10));
  else if(!strcmp(type, "i16")) err = nvs_set_i16(handle, key, strtol(value, nullptr, 10));
  else if(!strcmp(type, "u32")) err = nvs_set_u32(handle, key, strtoul(value, nullptr, 10));
  else if(!strcmp(type, "i32")) err = nvs_set_i32(handle, key, strtol(value, nullptr, 10));
  else if(!strcmp(type, "u64")) err = nvs_set_u64(handle, key, strtoull(value, nullptr, 10));
  else if(!strcmp(type, "i64")) err = nvs_set_i64(handle, key, strtoll(value, nullptr, 10));
  else if(!strcmp(type, "str"))
  {
    unescape(value);
    err = nvs_set_str(handle, key, value);
  }
  else if(!strcmp(type, "blob"))
  {
    uint8_t data[128];
    size_t size = strlen(value) / 2;
    if(size > sizeof(data)) { nvs_close(handle); return("Value too long"); }
    for(size_t i=0 ; i<size ; i++)
    {
      char hex[3] = { value[i * 2], value[i * 2 + 1], 0 };
      data[i] = strtoul(hex, nullptr, 16);
    }
    err = nvs_set_blob(handle, key, data, size);
  }
  else { nvs_close(handle); return("Unknown type"); }

  if(err == ESP_OK) err = nvs_commit(handle);
  nvs_close(handle);
  if(err != ESP_OK) return("Unable to write the value");
  restored++;
  return(nullptr);
}

int backupRestoreEnd()
{
  restoring = false;
  return(restored);
}

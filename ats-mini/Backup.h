#ifndef BACKUP_H
#define BACKUP_H

#include <Print.h>

// Backup of the settings, band states, memories, custom themes, station
// list choices and, optionally, the Wi-Fi networks, as text: one line per
// stored value, "<namespace> <key> <type> <value>", with % escapes.

void backupWrite(Print &out, bool withNetwork);

// Restore: begin, one line at a time, end. Each namespace found in the
// backup is cleared before its first value is written. Saving the
// settings stops until the receiver restarts.
void backupRestoreBegin();
// Returns an error message, or nullptr
const char *backupRestoreLine(const char *line);
// Returns the number of values restored
int backupRestoreEnd();

#endif // BACKUP_H

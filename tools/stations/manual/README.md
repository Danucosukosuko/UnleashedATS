# Manual station additions

One file per country, named after its ISO 3166 code (`ES.json`, `FR.json`...). Entries here take priority
over every other source when the lists are rebuilt.

```json
{
  "stations": [
    {"f": 98.1, "name": "LOS40", "city": "Madrid", "ps": "LOS40"},
    {"f": 91.2, "name": "Ràdio Arenys", "city": "Arenys de Mar"}
  ]
}
```

`f` is the frequency in MHz, `name` and `city` keep their own spelling, `ps` is the RDS name the station
sends (optional). Contributions are distributed under the GPLv3, like the rest of UnleashedATS.

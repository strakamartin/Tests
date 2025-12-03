```markdown
# QtTestMaker — Teacher / Student modes & immediate save

Změny v této verzi:
- Aplikace má dva módy: učitel (teacher) a student (student).
  - Učitel: spusť program s parametrem `-u` (např. `./QtTestMaker -u`). Toto rozhraní je upravovací (editor) — umožňuje vytvářet testy a otázky a vše se ukládá okamžitě do DB (bez tlačítka "Uložit změny do paměti" nebo "Načíst z DB"/"Uložit do DB").
  - Student: výchozí mód — zobrazí se v hlavním okně vlevo seznam testů. Student si vybere test a otázky se mu budou postupně zobrazovat přímo v hlavním okně (bez separátního dialogu).
- Všechny změny (název testu, popis, text otázky, typ, možnosti, odstranění/ přidání) se automaticky uloží do SQLite DB (DBManager).
- DB migrace: při spuštění se kontrolují a případně přidávají chybějící sloupce (test_id apod.) — zachována kompatibilita se starší DB.

Poznámky pro spuštění:
- Pro učitele: `QtTestMaker -u`
- Pro studenta: `QtTestMaker`

Doporučení:
- Před úpravami většího množství otázek raději zálohujte soubor DB.
```

```markdown
# QtTestMaker — Teacher / Student modes & immediate save

Změny v této verzi:
- Aplikace má dva módy: učitel (teacher) a student (student).
  - Učitel: spusť program s parametrem `-t` (např. `./QtTestMaker -t`). Toto rozhraní je upravovací (editor) — umožňuje vytvářet testy a otázky a vše se ukládá priebezne do DB.
  - Student: výchozí mód — zobrazí se v hlavním okně vlevo seznam testů. Student si vybere test a otázky se mu budou postupně zobrazovat přímo v hlavním okně (bez separátního dialogu).
- Všechny změny (název testu, popis, text otázky, typ, možnosti, odstranění/ přidání) se automaticky uloží do SQLite DB (DBManager).
- DB migrace: při spuštění se kontrolují a případně přidávají chybějící sloupce (test_id, student_count apod.) — zachována kompatibilita se starší DB.
- Počet otázek pro studenta: Učitel může v rozhraní nastavit počet otázek, které se studentovi zobrazí z testu (výchozí hodnota: 10). Tato hodnota se ukládá do DB a automaticky se načítá při spuštění testu studentem.

Poznámky pro spuštění:
- Pro učitele: `QtTestMaker -t`
- Pro studenta: `QtTestMaker`

Doporučení:
- Před úpravami většího množství otázek raději zálohujte soubor DB.
```

# QtTestMaker (SQLite edition)

Tato verze aplikace používá SQLite databázi místo JSON souborů. Data (otázky a možnosti) jsou ukládána do SQLite souboru, výsledky testů se ukládají také do DB. Po dokončení testu můžete výsledky otevřít ve výchozím poštovním klientovi (předvyplněný email) a odeslat je.

Hlavní vlastnosti:
- Typy otázek: single-choice, multiple-choice, text (přesný text)
- Editor otázek (přidávání/úprava/mazání)
- Uložení/načtení otázek v SQLite DB
- Spouštění testu: náhodný výběr podmnožiny otázek, náhodné pořadí možností
- Ukládání výsledků testu do DB (včetně detailů)
- Odeslat výsledky jako email (otevře výchozí mail klient s předvyplněným tělem)

Sestavení:
1) Potřebujete Qt 5 (Widgets + Sql) a CMake.
2) mkdir build && cd build
3) cmake .. && cmake --build .

Použití:
- Po spuštění se vytvoří/otevře `questions.db` v aktuálním adresáři.
- V levém panelu spravujte seznam otázek.
- Pro uložení změn do DB klikněte "Uložit do DB".
- Pro spuštění testu nastavte počet otázek a klikněte "Spustit test".
- Po dokončení testu si můžete nechat výsledky uložit do DB a/nebo je poslat emailem (otevře se výchozí mailový klient).

Poznámky:
- Pro robustní nasazení přidejte validace (např. single-choice má právě jednu správnou odpověď).
- Odesílání emailu v této ukázce používá `mailto:` (otevře výchozí poštovní aplikaci). Pokud chcete přímé odesílání přes SMTP z aplikace (včetně autentizace/STARTTLS), mohu přidat jednoduchého SMTP klienta nebo podporu konfigurace SMTP serveru.

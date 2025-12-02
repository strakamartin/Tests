```markdown
# QtTestMaker (multiple tests, SQLite edition)

Tato verze aplikace nyní podporuje více testů (sady otázek). Každý test má své ID, název a volitelný popis. Otázky jsou přiřazeny k testu přes test_id v databázi.

Hlavní novinky:
- Nová tabulka tests v SQLite.
- Otázky mají test_id -> umožňuje ukládat více oddělených testů.
- Uživatelské rozhraní: vytvářejte/odstraňujte testy, vyberte test a spravujte jeho otázky.
- Při spuštění testu vyberte, který test se bude zobrazovat studentovi.

Použití:
1) V levém horním rohu vyber nebo vytvoř test ( tlačítka Přidat test / Odstranit test ).
2) Uprav název a popis testu (pole pod výběrem). Pole se uloží při kliknutí na "Uložit do DB" nebo při opuštění editace.
3) Přidávej otázky pouze pro aktuálně vybraný test.
4) Klikni "Uložit do DB" pro uložení testu a jeho otázek.

Poznámky pro vývojáře:
- DBManager poskytuje nové metody: loadTests/addOrUpdateTest/removeTest/loadQuestionsForTest.
- Ukládání výsledků nyní zahrnuje test_id.
- Při migraci z předchozí verze: nová kolona test_id je přidána při inicializaci DB (v CREATE TABLE je sloupec přítomen).

Další možné vylepšení:
- Zobrazení seznamu výsledků pro každý test.
- Import/export testů (JSON / CSV / ZIP).
- Role uživatelů (zadavatel vs. student) a autentizace.
```

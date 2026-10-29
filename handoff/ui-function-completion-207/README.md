# Journal labels and Combat key display 207

Journal category headings, Current Quests and empty History now use the shared localized text table. Eight captions come from the selected original Chinese GlobalTxt; generic Notes is a project translation. The two remaining empty skill/effect labels reuse the existing localized None caption. Existing story headings keep their source text.

The text table contains 112 entries and still accepts the complete older 103-entry schema and earlier supported schemas. Only `engine.zip::data_tables/ui_text.txt` changes in the complete runtime; all other engine members and worlds are retained. Combat is displayed as implemented because its existing handler already toggles turn-based mode; this metadata change adds no new combat behavior.

Validation: combined native build, original text import and unsigned generic iOS compilation. No new unit cases, App replay, campaign, save or device operation. This is source/build acceptance; new target-iPad visual confirmation remains outstanding. Remote release 195 stays fixed.

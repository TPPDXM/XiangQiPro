Endgame positions adapted from Xiangqi Arena by dffge552.

Source: https://github.com/dffge552/xiangqi-pwa-offline

The original 12 custom levels and 300 added levels are jointly ordered by certified minimum red moves, then branching complexity. All titles were renamed. DataTable row names (L1–L312) remain stable identifiers even when their display positions change. The JSON table is the reimport source for ChessGenerationInfos.uasset; EndgameSources.json records original FENs, sources and grading. GameProgress stores its row order so existing unlocks, selected levels and personal move records can be migrated when the list is reordered.


November 2026 expansion: L313–L336 add 24 positions from basic-checkmates.json,
advanced-checkmates.json, endgames_all.json and extremely-challenging-endgames.json
in the same MIT-licensed source repository. New display titles are project-authored.
The exact minimum red moves (5–7) were certified by finite-horizon AND/OR search
using every legal attacker and defender move under FChessPosition rules. Positions
which exhausted the validation budget were excluded. Original FENs, source files,
original titles and grading are recorded in Content/DataTable/EndgameSources.json.

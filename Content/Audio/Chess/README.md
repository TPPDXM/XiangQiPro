# 棋子动作音效

正式 SoundWave 位于 Rook、Cannon、Knight、Royal、Advisor、Elephant、Pawn 子目录；Source 下的 WAV 是对应的可编辑音源，Originals 保留使用到的外部源文件。所有音效为 32 kHz、16 bit、单声道，使用 SC_ChessActions 限制同时播放 12 个声音。车移动声循环，走子结束淡出；其余均为单次声音，允许余响自然结束。声音独立于画面特效开关。

## 来源及许可

- 炮发射 S_CannonMove、爆炸 S_CannonCapture、普通落子 S_CannonLand：剪辑自 Thimras 的 Battle at sea，cannon_fire_1.ogg 与 cannon_hit_wall_no_splash.ogg。作者页面明确标注 CC0，可商用和修改，无强制署名要求。保留作者信息便于追溯。
  - https://opengameart.org/content/battle-at-sea
  - https://opengameart.org/sites/default/files/cannon_fire_1.ogg
  - https://opengameart.org/sites/default/files/cannon_hit_wall_no_splash.ogg
- 木轮滚动 S_RookMove、落子 S_RookLand：剪辑自 SFXMint 的 Stone Road Wooden Carriage Wheels 01。该站注明此素材由 AI 生成，按 CC0 1.0 提供，允许商业项目、修改与再分发。这里使用其木轮、轴架和石地接触声，并制作平滑循环接缝。
  - https://sfxmint.com/sounds/wooden-carriage-wheels-01
  - https://sfxmint.com/dl/wooden-carriage-wheels-01.wav
- S_RookCapture：为 XiangQiPro 原创制作的宽带木石碎裂拟音，没有使用外部样本或音乐。
- 马蹄：Max_Headroom 的 Horse Galloping.wav，作者说明是马在硬干地面上的实录，Freesound 页面标注 CC0。使用公开 HQ 预览音频，裁取清晰的单次硬地蹄触，组合为起跳双蹄、普通落地双蹄与吃子重踏；保留下载文件 HorseGallop_MaxHeadroom_HQ.mp3。
  - https://freesound.org/people/Max_Headroom/sounds/175356/
  - https://cdn.freesound.org/previews/175/175356_2861652-hq.mp3
- 嘶鸣：shadoWisp 的 horse neigh shortened.wav（原作 Horse_Whinny.wav，foxen10），两者 Freesound 页面均标注 CC0。使用公开 HQ 预览，保留约 1.47 秒完整嘶鸣；原始下载文件为 HorseNeigh_shadoWisp_HQ.mp3。
  - https://freesound.org/people/shadoWisp/sounds/269571/
  - https://freesound.org/people/foxen10/sounds/149024/
  - https://cdn.freesound.org/previews/269/269571_4564240-hq.mp3
- S_KnightMove 是实录双蹄起跳与嘶鸣的合成成品，约 1.55 秒；S_KnightLand 为实录双蹄落地，约 0.34 秒；S_KnightCapture 为实录重踏加原有少量碎土拟音，约 0.56 秒。S_KnightNeigh 为单独的嘶鸣资产，便于后续混音复用。无电子提示音或激光音色。
- CC0 1.0 说明：https://creativecommons.org/publicdomain/zero/1.0/

## 时序

Royal / Advisor / Elephant / Pawn 的 Move、Land、Capture 共十二段为 XiangQiPro 原创合成拟音，无新增外部样本：将帅为有自然余韵的铜钟，仕为短促钢刃响，相为沉稳木石闷响，兵为轻脚步、挥刀破风与金属碰撞。音源使用非整数倍钟/钢共振模态、衰减体腔和滤波接触噪声组合；没有电子提示音。兵的移动双步与实际到达的落脚声分开，吃子以刀剑碰撞代替落脚重音。

将帅三段音效已做离线频谱降噪，压低宽带杂音与低频轰鸣，柔化起音并在最后 120 ms 平滑收尾。保留原有铜钟共鸣、时长和播放设置，处理后的 WAV 已同步导入同名 SoundWave；游戏运行无需降噪脚本。相同内部时间窗的频谱对比中，900–2800 Hz 排除钟声共鸣附近频带后的能量下降约 9–13 dB，四个主要共鸣频带的能量变化小于 0.5 dB。

开始移动播放 Move；真实首次到达播放 Land 或 Capture，两者互斥。车轮声在整个实际移动期间循环，落点 70 ms 淡出；中断或棋子销毁会停止未完成移动声。炮声保留真实火药起音和自然余响，发射与爆炸分别在起点和落点触发。专用服务器不播放声音。

马普通走子的动作仍为 0.5 秒，嘶鸣独立保留自然余音；落地或吃子重踏由实际到达事件触发，空中不循环播放跑步声。

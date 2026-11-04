# 界面、倒计时与胜负音效（实录版）

全部音源已换为真实木块、鼓和古筝录音。只做截取、重采样、音量平衡、首尾淡化与乐句编排，不使用振荡器、电子提示音、合成笛箫或变调。Source 保留 48 kHz / 16 bit WAV；Originals 保留原录音和许可。六个 SoundWave 保持原资源路径，关闭循环、常驻加载，统一使用 SC_XQPEffects。

| 资源 | 时长 | 设计 |
| --- | --- | --- |
| S_UI_Click | 0.14 秒 | 真实轻木块单击 |
| S_UI_Select | 0.19 秒 | 稍厚的木块选择声 |
| S_Battle_Countdown | 0.50 秒 | 真实鼓膜敲击，每个倒计时数字一声，无语音 |
| S_Match_Victory | 3.50 秒 | 原音高古筝拨弦，上行短句 |
| S_Match_Defeat | 4.40 秒 | 原音高古筝拨弦，下行短句 |
| S_Match_Draw | 3.10 秒 | 古筝低回收束短句 |

录音来源：

- Pufermufin — LOVELY CHINESE GUZHENG PLUCKED.wav。作者说明以 Blue Spark、Blue Bluebird 电容麦克风和 Scarlett 6i6 录制真实古筝，按 CC0 发布。使用公开 HQ MP3，剪取独立拨弦编排短句，保留原有弦音与自然余响，没有变调。
  - 作者与许可：https://freesound.org/people/Pufermufin/sounds/396868/
  - HQ 录音：https://cdn.freesound.org/previews/396/396868_6977826-hq.mp3
  - 使用片段：17.215、24.285、29.955、38.165、43.965、53.825、74.475 秒附近的独立拨弦。
- Versilian Studios LLC — Versilian Community Sample Library，CC0。木块来自 Woodblock 的 wood_click_pp_rr1.wav 与 wood_click_mp.wav，鼓来自 Bass Drum 2 的 bassdrum_hit_mp1.wav。完整许可保存在 Originals/VCSL_LICENSE.txt。
  - 项目与许可：https://github.com/sgossner/VCSL
  - 木块：https://github.com/sgossner/VCSL/tree/master/Idiophones/Struck%20Idiophones/Woodblock
  - 鼓：https://github.com/sgossner/VCSL/tree/master/Membranophones/Struck%20Membranophones/Bass%20Drum%202

按钮沿用 Slate 有效点击回调；程序初始化、恢复下拉默认值不播放选择音。公共界面、动态列表条目和嵌套按钮均在原控件树配置声音，不改视觉样式。倒计时由 BattleStartComponent 单点去重，联机校时不会重复敲同一数字，重开重新计数。胜负音由 GameState 的正式终局路径触发，按本机阵营判断；观战和单骑积分挑战不播放个人输赢声，重新查看结算界面不重播。

背景音乐通过 Camera2PActor 既有 GamePause / GameResume 接口暂停、恢复读头。设置子菜单不会提前恢复；关闭完整暂停菜单后才继续。仅打开本机菜单、不暂停网络棋局时也适用。

# 炮的两段式 Niagara 表现

棋子基类的 `MoveEffects` 组件提供通用触发入口。各项资源默认留空；目前只有 `AChess_Pao` 配置了下面的正式资源。其他棋子可在派生类或蓝图中分别配置 PC 与移动端资源。

| 资源 | 用途 |
| --- | --- |
| NS_CannonMoveTrail | PC 移动拖尾：Local Space Ribbon、World Space 火星、Smoke、HotGas |
| NS_CannonCapture | PC 吃子：Flash、Shockwave、GPU Sparks、Mesh Debris、Smoke、HotGas |
| NS_CannonMoveTrail_Mobile | 移动端 CPU 拖尾，较低发射预算 |
| NS_CannonCapture_Mobile | 移动端全 CPU 吃子，减少火星、碎片和烟雾 |
| NS_CannonLandingFlash | 普通落子的单粒子余烬闪点；吃子时由命中系统自己的 Flash 负责 |

## 触发与生命周期

普通对局和烽火对局均在实际移动开始时创建拖尾。在首次到达目标棋子上方时停发拖尾并创建独立吃子实例，后续悬停、下落和重复网络快照不再次触发。同一棋子连续移动会创建新拖尾；旧实例脱离棋子后自然结束。命中原点读取目标格的棋盘坐标，连续叠放吃子时冲击波也保持贴地。

所有实例使用 Niagara AutoRelease 对象池。停止移动只设置 `User.Emit=0` 并执行允许存量粒子完成的 Deactivate，不使用强制销毁定时器。连续吃子不会复位或抢占先前实例。

Flash 寿命 0.05 秒，Shockwave 0.3 秒，Sparks 0.75 秒，Debris 0.85 秒，Smoke 1.25 秒，HotGas 0.9 秒。吃子系统的视觉结束以最后一个粒子完成为准。拖尾 Ribbon 寿命 0.4–0.6 秒、火星 0.45–0.7 秒、烟雾 0.95–1.25 秒、热气 0.65–0.9 秒，也都自然消散。

## 外部参数

吃子保留 `User.HitLocation`（Vector）、`CaptureColor`（LinearColor）、`FireDirection`（Vector）、`ImpactScale`（Float）、`Intensity`（Float）。HitLocation 经标准 Vector→Position 模块转换以支持 Niagara 世界坐标。PC 火星沿世界向上方向的 55° 随机锥发射，速度 40–96 cm/s；移动端沿棋盘平面行进方向的 65° 随机锥发射，速度 34–82 cm/s。纯竖直的到达动画不会覆盖已采样的平面方向。碎片读取被吃棋子的阵营色，初速 Z 为 36–64 cm/s。

拖尾保留 `User.MoveSpeed`、`MaxSpeed`、`TrailColor`、`Emit`。另使用 `MoveDirection` 和 `LocalMoveDelta` 驱动世界空间火星方向及局部条带历史点补偿。速度控制火星发射率、条带宽度和亮度；尺寸统一由 ImpactScale 调节。独立厘米尺寸不继承棋子模型缩放。

通用质量参数为 `User.Quality`、`Mobile`、`Intensity`、`EnableSmoke`、`EnableDebris`。自定义棋子资源可沿用同一接口。

## 质量与预算

| 档位 | PC 粒子预算系数 | 移动端预算系数 |
| --- | ---: | ---: |
| 低 | 0.22 | 0.25 |
| 中 | 0.42 | 0.40 |
| 高 | 0.70 | 0.55 |
| 超高 | 1.00 | 0.70 |

PC 读取现有“特效质量”；移动端及竖屏预览读取现有“画面质量”的整体档位。低档烟气预算为 65%，其余档位为 100%；PC 中档、移动端高档起启用碎片。命中烟雾与热气各保留至少一粒，短距离走子也有最低烟气发射率（仅移动时启用），避免速度系数与低画质预算相乘后完全看不到。烟气不使用相机距离裁剪，碎片仍保留距离裁剪。HotGas 使用独立的动画薄雾材质，桌面包含法线折射，移动端以有色薄雾表达热浪，不依赖屏幕折射才能看见。

PC 最高档单次吃子预算为 178 粒子（1+1+120+28+20+8）。拖尾按移动距离采样，采样速度上限 120 cm/s。移动端使用独立 CPU 系统和更低的基础数量。全部系统与发射器设置 Fixed Bounds；GPU 发射器仅用于 PC 吃子火星。

这些是单实例预算；短时间大量吃子允许自然重叠，因此场景总粒子数会随并发实例数增加。

## 玩家开关

双端“画面设置”列表中的“棋子移动与吃子特效”统一控制拖尾、落子闪点和吃子系统，默认开启。设置保存在本机 `GameUserSettings.ini` 的 `[XQP.Graphics] ChessEffectsEnabled`，与画质档位独立。关闭时立即停止活动拖尾发射，存量粒子自然结束；不修改走子状态、吃子规则或网络同步。

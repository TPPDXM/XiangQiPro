# 音频设置

设置菜单通过 PC 与移动端各自的 `BP_UI_AudioSettings` 页面使用既有下拉菜单控件。背景音乐、音效分别提供关闭／开启选项；仅开启时显示相应音量条。两端保留各自字体、配色、边框和返回按钮，移动端增加滑块触摸区域。

`UXQPAudioSettingsSubsystem` 为每个 GameInstance 管理设置。`SC_XQPMaster` 下分 `SC_XQPMusic` 和 `SC_XQPEffects`，由 `SM_XQPUserSettings` 独立调节增益。背景音乐保留原有场景音量，再乘用户音量；关闭分类时只把增益调为零，保留上次音量。循环音乐采用 PlayWhenSilent，重新开启时继续播放。

拖动音量条立即试听，松开鼠标、手柄完成拖动或离开界面时保存。下拉开关立即保存。持久化位置为用户 `GameUserSettings.ini` 的 `[XQP.Audio]` 节：`MusicEnabled`、`EffectsEnabled`、`MusicVolume`、`EffectsVolume`。音量范围 0–1，默认开启且音量为 1。加载新地图时重新应用到所属音频设备，避免场景切换丢失设置。

资源位于项目既有 `/Game/Audio` Cook 目录内。`DefaultEngine.ini` 的默认 SoundClass 指向音效分类；Lobby、BattleMusic 明确使用音乐分类，其余项目声音明确使用音效分类。

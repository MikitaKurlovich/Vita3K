# Типичные дыры GXM на Vita3K (Pi 5 / Vulkan)

Срез 2026-09-07, правки lab после этого среза.

Железо: Raspberry Pi 5, Mesa 25.3.6, `V3D 7.1.10.2`, HDMI 4K60. Пример симптомов: homebrew `SBWF12122` Subway Surfers (Unity 5.6.6). Это не регресс EXIDX/`segment.perms`.

В lab (этот checkout) закрыто в коде, для **всех** тайтлов, не XML:

1. `sceGxmSetWClampEnable/Value` и `SetWBufferEnable` пишут state; VS клампит `|w|`; W-buffer идёт как `1/w`; Vulkan `depthClampEnable`.
2. Async pipeline больше не скипает draw: `retrieve_pipeline` ждёт готовности. На тестовом Pi `async-pipeline-compilation: false`, но глобальный дефолт не менялся.
3. При `v-sync: true` present только FIFO, не Mailbox.
4. `GetPassType` считается из discard/blend/depth-replace; `IsEnabled` = `!has_no_effect()`.
5. На тестовом Pi `high-accuracy: true`; это настройка конкретного устройства, не новый глобальный дефолт. Шейдер-кэш `CURRENT_VERSION` 14.

Ниже — как это ломалось на бинаре `2f030cd` и почему конфиг Subway однажды не сработал. Не делать: Vulkan→OpenGL, `fps-hack`, глотать unimplemented.

## Как это устроено на Vita

GPU: PowerVR SGX543MP4+, tile-based deferred renderer.

Экран режется на тайлы 32×32 (`SCE_GXM_TILE_SIZEX/Y`). Vertex USSE пишет parameter buffer. ISP на тайле делает hidden surface removal **до** fragment USSE: затеняются только видимые сэмплы. Depth/stencil живут в on-chip памяти тайла, не в framebuffer fetch как на PC.

Два режима глубины:

- Z-buffer: обычный `z/w` после перспективы. Точность падает у дальнего плана.
- W-buffer: в ISP кладут `1/w`. Ближний план точнее, дальний хуже. Unity 5.x на Vita часто включает W-clamp, чтобы `w → 0` у near plane не взрывал интерполяцию.

`sceGxmSetWClampEnable` / `sceGxmSetWClampValue` ограничивают `w` снизу. Без clamp спекуляр `pow(N·H, shininess)` на рельсах, металле, мокром асфальте даёт блик, который прыгает кадр-к-кадру: это не «битая текстура», это разъезд `w` в varyings.

Цветовой путь: линейный render target, гамма на scanout (`sceGxmColorSurfaceSetGammaMode` / `sceGxmTextureSetGammaMode`). Dither на color surface маскирует бандинг 8-бит. Mid-scene flush (`sceGxmMidSceneFlush`) гоняет ISP между проходами Unity (opaque → alpha → overlay), флаги говорят, ждать ли vertex.

Настоящая Vita рисует 960×544, 30 FPS у этого порта (автор: до 45 только с разгоном CPU/GPU).

## Как это ломает Vita3K

Vita3K переводит GXP USSE в SPIR-V и рисует Vulkan immediate pipeline (у V3D тайлы свои, 64×64, и это **другой** TBDR, не SGX).

Глубина: `screen_z = z_offset + z_scale * (z / w)`, затем `z = max(z, 0)`. Clamp отрицательного Z есть. W-clamp **нет**. В `GxmRecordState` полей `w_clamp_*` нет. Три HLE:

```
sceGxmSetWBufferEnable   UNIMPLEMENTED  (даже не пишут в state)
sceGxmSetWClampEnable    UNIMPLEMENTED
sceGxmSetWClampValue     UNIMPLEMENTED
```

Vulkan-устройство просит `depthClamp`, но rasterizer pipeline ставит только `depthBiasEnable = TRUE`. `depthClampEnable` не выставляется. То есть даже грубый аналог W-clamp в API хоста выключен.

`sceGxmFragmentProgramGetPassType` всегда врёт `OPAQUE`. Прозрачный/mask pass Unity может уйти не в тот путь.

`sceGxmMidSceneFlush`: флаги игнорятся. `sceGxmColorSurfaceGetDitherMode` и часть clip API не реализованы.

Async pipeline (по умолчанию ON): draw **пропускается**, пока SPIR-V не собран. Дыры в мешах, вспышки «пустых» материалов. Это не GXM, это эмулятор.

High accuracy OFF: `texture viewport` вместо точного fetch. Лог: `using texture viewport for better performance`. Шейдер interlock (имитация framebuffer fetch SGX) не включается. Боксы, блики, overlapping decals.

Anisotropic = 1: min/mag linear, mip linear, AF выключен. На движущейся земле и вагонах mip-shimmer выглядит как блестки на текстуре.

Present: порядок выбора Mailbox > FIFO_RELAXED > FIFO. Mailbox отбрасывает кадры. Плюс bilinear растяжение 960×544 на 4K. Яркий спекуляр сэмплится с другого текселя каждый кадр.

Swapchain: `VK_COLOR_SPACE_SRGB_NONLINEAR`. Если игра уже пишет «гамма на поверхности», хост делает гамму второй раз: блики горят, тени чернеют.

## Каталог симптомов

Каждая строка: что видит игрок, что на SGX, что делает Vita3K, чем подтверждено, чинится ли конфигом.

### 1. Блики на текстурах (sparkle / specular crawl)

Игрок: металл, рельсы, мокрый пол, персонаж вспыхивают точками при движении камеры.

SGX: W-clamp + TBDR ISP; спекуляр считается один раз на видимый сэмпл тайла.

Vita3K: `sceGxmSetWClamp*` дропается; `w` гуляет; `pow` взрывается. Mailbox+4K усиливает.

Доказательство `SBWF12122`: оба WClamp UNIMPLEMENTED на старте сцены. `GxmRecordState` не хранит clamp. Rasterizer без `depthClampEnable`.

Конфиг не лечит. Нужен код: записать enable/value в record, включить `depthClampEnable`, в VS clamp `w` (и/или `1/w`) до интерполяции. Отдельно не путать с AF.

### 2. Дыры в 3D, «не всё прорисовалось»

Игрок: вагон/персонаж без куска меша, через кадр появляется.

SGX: шейдер уже в USSE binary, компиляции на ходу нет.

Vita3K: async compilation, 1 поток на Pi. Пока pipeline нет, draw skip. Кэш `shaders/SBWF12122` дописывался минутами (`save_pipeline_cache` 22:21…22:28).

Конфиг: `async-pipeline-compilation: false` на **этом** тайтле. Первый заход после сброса кэша подлагивает, меши полные.

### 3. Z-fight, полигоны мигают плоскостью

Игрок: декали, пути, overlay UI-на-мире, два coplanar меша.

SGX: ISP pixel-accurate HSR; polygon offset через depth bias unit/slope.

Vita3K: depth bias dynamic state есть (`eDepthBias`). Точность зависит от формата depth. Если нет D24S8/D32S8, падает в `D16Unorm` (`Your device doesn't support standard deep stencil`). 16 бит на 960×544 с Unity near/far даёт fight.

Конфиг: `high-accuracy: true` (выключает texture viewport). Не заменяет D24.

### 4. Пропавшие/чёрные текстуры, «боксы»

Игрок: персонаж силуэт, огонь квадратами, UI без атласа.

SGX: framebuffer fetch дешёвый (тайл в on-chip).

Vita3K без shader interlock: overlapping additive/alpha врёт. Macdu: Soul Sacrifice fire boxes, MK. На V3D interlock часто нет, high-accuracy тогда только отключает texture viewport.

Отдельно: Unity ищет `*.resG`/`*.resS` (GPU-specific). На Vita их нет, stat miss в логе. Это fallback на base assets, не дыра эмулятора.

### 5. Shimmer земли и стен (не спекуляр)

Игрок: текстура «плывёт» пикселями на полу при беге.

SGX: HW AF, точный mip lod, lod bias в заголовке текстуры.

Vita3K: `mipLodBias = (lod_bias - 31) / 8`, `anisotropyEnable` только если AF>1 и фильтр не nearest. Глобально AF=1.

Конфиг: AF 8× или 16× на 3D-тайтле. Не путать со спекуляром.

### 6. Низкий FPS

Игрок: «не 60».

Две полки:

- Игра/порт. Subway Surfers Vita: 30 штатно.
- Эмулятор. `cpu-opt: false` выключает Dynarmic `all_safe_optimizations` и page table. На Pi5 Unity это лишние десятки процентов CPU. В lab это оставили после abort Papers Please; abort был EXIDX, не JIT.

Ещё: `dump-elfs` / `log-ehabi` на каждый тайтл, 4K present, async compile на том же CPU.

Не включать `fps-hack`: он ускоряет **время игры**, не GPU.

### 7. Tearing / дрожание кадра

Mailbox отбрасывает готовые кадры. VSync в YAML (`v-sync: true`) на Vulkan present mode **не выбирает FIFO**. Код: `screen_renderer.cpp`, Mailbox первым.

На 4K TV плюс bilinear upscale. Лечится FIFO (код) или 1080p HDMI (система), не переключателем OpenGL.

### 8. Двойная гамма, «выжженные» блики

Swapchain `eSrgbNonlinear`. Vita часто пишет линейный RT и ставит gamma mode на surface. Если Vita3K применяет sRGB sampling (`texture.gamma_mode` → `linear_to_srgb`) **и** present уже в sRGB space, спекуляр клипит в белую кашу, которая мерцает на границе клипа.

Смотреть `is_gamma_corrected` в pipeline и `sceGxmColorSurfaceSetGammaMode`.

## Почему конфиг Subway не сработал 7 сентября

Batocera кормит `-c /userdata/system/configs/vita3k/config.yml` (строчная папка).

Vita3K `config_path` для XML: XDG `…/configs/Vita3K` (заглавная). Лог: `User config path: /userdata/system/configs/Vita3K`.

Per-game файл: `config_path/config/config_<TitleId>.xml`. Его клали в `vita3k/config/`. Загрузчик `-r SBWF12122` XML не видел.

Поэтому в прогоне 22:36 снова:

- `using texture viewport`
- `Enabling asynchronous pipeline compilation`
- `CPU Optimisation state: true` (это из YAML, его `-c` читает)

`Custom configuration file loaded successfully` это YAML, не XML тайтла.

Правильный путь XML: `/userdata/system/configs/Vita3K/config/config_SBWF12122.xml`.

## Чеклист на любой 3D тайтл (этот Pi)

Лог старта:

1. `Vulkan device`, `Present mode`, `texture viewport` vs `shader interlock`, `memory mapping`, `CPU Optimisation`, `asynchronous pipeline`.
2. `Unimplemented sceGxmSetWClamp*` / `SetWBufferEnable` / `MidSceneFlush` / `GetPassType`.
3. `doesn't support standard deep stencil` → D16, ждать z-fight.
4. Повторный `save_pipeline_cache` минутами → async дыры.

Конфиг (не код):

- `cpu-opt: true` глобально, пока нет доказательства JIT-багов на этом тайтле.
- Дампы EHABI/ELF только через wrapper Papers Please, не в YAML.
- На тайтл: XML в `Vita3K/config/`, `high-accuracy=true`, `async-pipeline-compilation=false`, AF 8× если shimmer пола.
- Resolution multiplier 1. Upscale 2× на V3D убивает fillrate и множит ошибки depth.

Не делать:

- Менять Vulkan на OpenGL «чтобы блики прошли».
- `fps-hack`.
- Глотать unimplemented GXM (`return 0` без state).
- Лечить EXIDX/FIOS/NGS: другой слой.

## Очередь кода (GXM, не lab)

Сделано в этом checkout (HLE + шейдер + Vulkan). Производительные настройки остаются конфигурацией тайтла/устройства. Остаётся не код этого слоя:

5. Dither color surface: ordered dither в fragment, иначе бандинг читается как sparkle.
6. Mid-scene flush: флаги кроме `PRESERVE_DEFAULT_UNIFORM_BUFFERS` по-прежнему только STUB.

Проверка после деплоя: banner не `2f030cd`; `Present mode: Fifo`; нет `texture viewport` / `asynchronous pipeline`; нет `Unimplemented sceGxmSetWClamp*`. Первый заход пересоберёт `vk14-*.spv`. Subway рельсы без точечных вспышек при AF=1 (чтобы не спутать с mip). Потом AF 8× отдельно на shimmer пола.

## Привязка к тайтлам

| Title | Движок | Что типично |
|---|---|---|
| SBWF12122 Subway Surfers | Unity 5.6.6 homebrew | WClamp stub + async skip + AF=1 + Mailbox 4K. Не retail Vita. |
| PCSE01056 Papers, Please | Haxe/OpenFL 2D | GXM почти не этот каталог. Слой 0: EXIDX/`perms`. |
| Unity retail (PCSB00643 и т.п.) | Unity Vita | Тот же WClamp/pass type/async. |
| PhyreEngine 3D | Phyre | Чаще surface cache / memory mapping, чем WClamp. |

## Файлы в дереве

- `vita3k/modules/SceGxm/SceGxm.cpp`: WClamp/WBuffer/MidSceneFlush/GetPassType
- `vita3k/renderer/include/renderer/types.h`: `GxmRecordState` без w-clamp
- `vita3k/renderer/src/vulkan/pipeline_cache.cpp`: rasterizer, depth test always on, нет depthClamp
- `vita3k/shader/src/spirv_recompiler.cpp`: `z_offset + z_scale * (z/w)`, clamp `z<0`
- `vita3k/renderer/src/vulkan/texture.cpp`: lod bias, AF
- `vita3k/renderer/src/vulkan/screen_renderer.cpp`: Mailbox, depth format, sRGB surface
- `vita3k/renderer/src/vulkan/renderer.cpp`: high-accuracy → interlock vs texture viewport
- `vita3k/config/src/settings.cpp`: XML `config_<id>.xml` от `config_path`, не от `-c` YAML

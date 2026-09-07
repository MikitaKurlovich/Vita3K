# PCSE01056 (Papers, Please USA), план на 2026-09-07

Форк: `MikitaKurlovich/Vita3K`, ветка `pcse01056-lab`. Апстрим: `Vita3K/Vita3K` `master`. Тег `lab-496939b` не трогать. На Pi не трогать `Vita3K.bak-4074`.

Этот файл заменяет два рабочих документа: test-plan (`~/.gstack/projects/ps1/pcse01056-test-plan-20260907.md`) и architecture holes (Cursor plan `pcse01056_architecture_holes_e1959940`). Старый autoplan (CEO/Eng/DX) остаётся архивом в `~/.gstack/projects/ps1/ceo-plans/`. Здесь только текущее состояние и обязательный хвост.

## Что уже закрыто

Слой 0 (EHABI) закрыт in-game 2026-09-07 ~22:03 CEST, New Game, первый посетитель.

Доказательство в `/userdata/system/cache/Vita3K/vita3k.log`:

- `[EHABI] module=Main` / `SceLibc` `relocated=yes`
- `unwind-bind remaining-svc=0` после `libc.suprx`
- throw `Invalid field:face` в LLE `__cxa_throw` (`GetModuleInfoByAddr addr=0x80353F67 module=SceLibc seg0_perms=0x8000005`)
- следующий кадр eboot (`addr=0x81157991 module=Main seg0_perms=0x5`)
- ноль `guest abort dump`, ноль `sceKernelCallAbortHandler`, ноль `hle __cxa_throw reached`
- после throw открываются паки документов (`0001.pak`, `0022.pak`, …), процесс жив

Корень abort: Sony libc кэширует EXIDX только у сегмента с `perms & 1`. Vita3K оставлял `perms=0`, bsearch шёл по пустой таблице, unwind не выходил из libc. Фикс: `segment.perms = p_flags`; `GetModuleInfoByAddr` копирует полный `SceKernelModuleInfo` (libc не пишет `size`).

Код слоя 0, который уже в ветке:

- relocate EXIDX/EXTAB, offset 0 валиден, sentinel только `0xffffffff`, ExclusiveEnd может равняться `seg_size`
- `normalize_ehabi_range` / `normalize_extab_range` (EXTAB не проверять на `% 8`)
- `copy_module_info_by_addr` snapshot под одним lock
- аудит unwind-NID после libc; HLE `__cxa_throw` стоп только если LLE libc с кодом и NID остался svc
- TLS: первый writer (eboot) побеждает
- FIOS overlay GetInfo/Modify/Remove/ResolveSync02 + per-thread SetDisabled02
- AppUtil Init/workBuf; NGS `system->flags`
- host `ctest -R '^(kernel|util)$'` зелёный (47 kernel + util)
- Pi: `make build/package/deploy/rollback`, RUNPATH `$ORIGIN/../lib`, Qt 6.11 из squashfs

Инварианты (не ломать):

- не глотать `__cxa_throw`, не NOP abort, не патчить eboot без причинности
- `UNIMPLEMENTED()` = `return 0`; «нет abort» само по себе не закрывает слой
- `external/sdl` не коммитить

## Обязательный хвост

### P0: апстрим PR-1 (exidx-va)

Черновик: `docs/upstream-pr1-exidx.md`. Открывать на чистой ветке от `upstream/master`, без lab (flight recorder, AppUtil/NGS game-path, abort dump).

В PR должны войти и поздние куски слоя 0, без которых in-game не жил:

- `segment.perms` из ELF `p_flags`
- `copy_module_info_to_guest(..., force_full)` для ByAddr
- `normalize_extab_range`

Матрица регресса, homebrew F2, `ctest`, clang-пресет `ci-linux-clang-appimage`, ссылки #305 / #3047. Коммерческий образ в CI не класть.

Не мешать в этот PR: FIOS CRUD, TLS first-writer, NGS flags, AppUtil, Pi Makefile.

### P1: будка как продукт

1. Повторный запуск с Saved Data: freeze splash ([Vita3K#3047](https://github.com/Vita3K/Vita3K/issues/3047)). Пока сейв есть, прогон слоя 0 только New Game.
2. Ввод на Pi: drag-and-drop документов геймпадом/мышью (на Vita это тач). Без этого «играбельно» ложно даже при живой будке.
3. FIOS per-thread на живой трассе: `SetDisabled02` потока A не должен ломать `ResolveWithRangeSync02` потока B. Код есть, in-game лог `[FIOS] tid/disabled/in/out` ещё не снят.

### P2: после стабильной будки

1. Лица и документы глазами: Vulkan Pi и OpenGL Mac, не лечить бэкендом.
2. Перф Pi: fps/аудио до и после диагностики, модель платы в логе. Цель: дельта не больше ~1%.
3. `getenv` / RTC / lang: дешёвая проверка. Если throw-PC не двигается, закрыть записью «не корень».

### P3: полный образ

День N, Endless, trophy / NP Score, CommonDialog, сцены из `gamestates.gsb`. Только после P1.

## Не обязательно / не корень abort

`Invalid field:face` бросается и ловится. Это штатный Haxe try/catch, не баг эмулятора. Статика `__SetField` / Faces.xml нужна только если лица пустые при живом catch.

Var-import eboot rebound в libc (stdin/errno/typeinfo). `0x80002000` в регистрах и host `Unhandled SIGSEGV` (dynarmic) в логе были и после слоя 0; к abort первого посетителя не привязаны. Чинить, если снова упрётся PC в `0x80002xxx` или `r0=0xDEADBEEF` на горячем GXM-пути.

NID `0x99EEBD1F` (sysmodule var) не rebound. Не трогать, пока нет PC.

TSan полного `KernelState` load/unload libc: отдельный контур, не Mac RelWithDebInfo.

Старый SDK homebrew + LLE libc (`degenerate-pair`) и HLE-libc игра: регресс для PR-1, не блокер будки PCSE01056.

## Тест-чеклист

Host (закрыто):

- [x] `relocate_module_info_offset` / `normalize_ehabi_range` / `normalize_extab_range` / `exidx_from_phdr` / `select_exidx_range` / `segment_contains` / `copy_module_info_to_guest` / `segment_is_executable`
- [x] ByAddr `force_full`: guest size `0x40` всё равно копирует `segments[].perms`
- [x] snapshot-под-lock vs unload; flight recorder wrap
- [x] util `trim_copy`

In-game (Pi Vulkan, New Game):

- [x] `[EHABI] relocated=yes` Main и SceLibc
- [x] `unwind-bind remaining-svc=0` после libc
- [x] первый посетитель без `guest abort dump`
- [x] throw из libc: `GetModuleInfoByAddr` SceLibc, затем Main `0x81157991`
- [ ] homebrew старый SDK + LLE libc
- [ ] HLE-libc игра, поведение как раньше
- [ ] искусственно: HLE `__cxa_throw` при LLE + unbound → `LOG_CRITICAL` + стоп потока
- [ ] Continue с сейвом, нет freeze splash (#3047)
- [ ] FIOS per-thread A vs B
- [ ] drag-and-drop документов
- [ ] fps/аудио, дельта ≤ 1%

Откат: `make rollback PI=<host> TAG=4074`. Runbook: `docs/pi-runbook.md`.

## Порядок дальше

1. Играть будку; если лица/документы живые, слой 1 не открывать.
2. P1: #3047, тач, FIOS-трасса.
3. Cherry-pick слоя 0 на `upstream/master`, PR-1. Не ждать кампании.
4. P2 визуал/перф, потом P3.

Голоса ревью: Composer 2.5 + Grok 4.6 (`composer+grok`), не Codex.

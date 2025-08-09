# Инструкции по сборке

## Предварительные требования

### Обязательное ПО

1. **Visual Studio 2022** (любая редакция - Community, Professional, Enterprise)
   - Компоненты MSVC v143
   - Windows 10/11 SDK (последняя версия)
   - CMake Tools for Visual Studio

2. **CMake 3.20+**
   - Можно установить через Visual Studio Installer
   - Или скачать с [cmake.org](https://cmake.org/download/)

3. **Git**
   - Для клонирования vcpkg и проекта

### Среды разработки

Поддерживаемые среды:
- **Visual Studio 2022** (рекомендуется)
- **Visual Studio Code** с C++ расширениями
- **CLion**
- **Qt Creator**

## Установка vcpkg

### Шаг 1: Клонирование vcpkg

```bash
# Клонирование в папку C:\vcpkg
git clone https://github.com/Microsoft/vcpkg.git C:\vcpkg
cd C:\vcpkg

# Запуск bootstrap скрипта
.\bootstrap-vcpkg.bat
```

### Шаг 2: Интеграция с Visual Studio

```bash
# Интеграция с Visual Studio
.\vcpkg integrate install

# Установка переменной окружения (опционально)
setx VCPKG_ROOT C:\vcpkg
```

### Шаг 3: Проверка установки

```bash
# Проверка версии
.\vcpkg version

# Поиск пакета (пример)
.\vcpkg search skia
```

## Сборка проекта

### Метод 1: Через Visual Studio

1. **Открытие проекта**:
   - Запустить Visual Studio 2022
   - File → Open → CMake...
   - Выбрать `CMakeLists.txt` в корне проекта

2. **Настройка CMake**:
   - Открыть Project → CMake Settings
   - Добавить параметр в CMake command arguments:
     ```
     -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake
     ```

3. **Сборка**:
   - Build → Build All
   - Или нажать Ctrl+Shift+B

### Метод 2: Через командную строку

```bash
# Клонирование проекта
git clone <repository-url> window_winapi
cd window_winapi

# Создание папки для сборки
mkdir build
cd build

# Конфигурация CMake
cmake .. -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake

# Сборка (Release)
cmake --build . --config Release

# Сборка (Debug)
cmake --build . --config Debug
```

### Метод 3: Использование preset'ов

Если в проекте есть `CMakePresets.json`:

```bash
# Просмотр доступных preset'ов
cmake --list-presets

# Конфигурация с preset'ом
cmake --preset=windows-x64-release

# Сборка
cmake --build --preset=windows-x64-release
```

## Установка зависимостей

### Автоматическая установка

vcpkg автоматически установит все зависимости из `vcpkg.json` при первой сборке.

### Ручная установка (опционально)

```bash
cd C:\vcpkg

# Основные пакеты
.\vcpkg install skia:x64-windows
.\vcpkg install angle:x64-windows
.\vcpkg install vulkan:x64-windows
.\vcpkg install directxtk12:x64-windows
.\vcpkg install directxtk:x64-windows
.\vcpkg install directxmath:x64-windows
.\vcpkg install d3d12-memory-allocator:x64-windows

# Дополнительные пакеты
.\vcpkg install imgui[dx12-binding,dx11-binding,vulkan-binding,win32-binding]:x64-windows
.\vcpkg install glfw3[vulkan]:x64-windows
.\vcpkg install fmt:x64-windows
.\vcpkg install spdlog:x64-windows
.\vcpkg install nlohmann-json:x64-windows
```

## Опции сборки

### Конфигурации

- **Debug**: Отладочная версия с отладочной информацией
- **Release**: Оптимизированная версия для продакшена
- **RelWithDebInfo**: Оптимизированная с отладочной информацией
- **MinSizeRel**: Минимальный размер файла

### Опции CMake

```bash
# Включение конкретных фич
-DENABLE_VULKAN=ON          # Поддержка Vulkan
-DENABLE_DIRECTX12=ON       # Поддержка DirectX 12
-DENABLE_DIRECTX11=ON       # Поддержка DirectX 11
-DENABLE_SKIA_GPU=ON        # GPU ускорение Skia
-DENABLE_EXAMPLES=ON        # Сборка примеров
-DENABLE_TESTS=ON           # Сборка тестов

# Отладка и профилирование
-DENABLE_PROFILING=ON       # Профилирование
-DENABLE_LOGGING=ON         # Логирование
-DENABLE_ASSERT=ON          # Проверки в runtime
```

### Пример полной конфигурации

```bash
cmake .. \
  -DCMAKE_TOOLCHAIN_FILE=C:/vcpkg/scripts/buildsystems/vcpkg.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DENABLE_VULKAN=ON \
  -DENABLE_DIRECTX12=ON \
  -DENABLE_DIRECTX11=ON \
  -DENABLE_SKIA_GPU=ON \
  -DENABLE_EXAMPLES=ON
```

## Запуск примеров

После успешной сборки:

```bash
# Переход в папку сборки
cd build

# Запуск базового примера
.\examples\Release\basic_window.exe

# Запуск примера с API
.\examples\Release\graphics_apis_demo.exe

# Запуск примера со слоями
.\examples\Release\layer_system_demo.exe
```

## Интеграция в существующий проект

### Метод 1: Как подмодуль

```cmake
# Добавление в CMakeLists.txt
add_subdirectory(window_winapi)

# Линковка с вашим проектом
target_link_libraries(your_project PRIVATE window_winapi)
```

### Метод 2: Как установленная библиотека

```bash
# Установка библиотеки
cmake --build . --target install

# Использование в другом проекте
find_package(window_winapi REQUIRED)
target_link_libraries(your_project PRIVATE window_winapi::window_winapi)
```

## Решение проблем

### Ошибка: Не найден vcpkg

```bash
# Проверьте путь к vcpkg
dir C:\vcpkg\scripts\buildsystems\vcpkg.cmake

# Укажите правильный путь
cmake .. -DCMAKE_TOOLCHAIN_FILE=<путь-к-vcpkg>/scripts/buildsystems/vcpkg.cmake
```

### Ошибка: Не найден пакет

```bash
# Проверьте установленные пакеты
.\vcpkg list

# Принудительная установка
.\vcpkg install <package-name>:x64-windows --recurse
```

### Ошибка: DirectX SDK

```bash
# Установите Windows SDK через Visual Studio Installer
# Или скачайте с developer.microsoft.com
```

### Ошибка: Vulkan SDK

```bash
# Скачайте Vulkan SDK с vulkan.lunarg.com
# Установите переменную окружения VULKAN_SDK
setx VULKAN_SDK "C:\VulkanSDK\<version>"
```

## Проверка установки

После сборки проверьте:

1. **Наличие файлов**:
   - `build/Debug/window_winapi.lib` или `build/Release/window_winapi.lib`
   - Примеры в `build/examples/Debug/` или `build/examples/Release/`

2. **Запуск примера**:
   ```bash
   .\build\examples\Release\basic_window.exe
   ```

3. **Проверка зависимостей**:
   Пример должен запуститься без ошибок о отсутствующих DLL.

Подробную информацию о API и примерам см. в [README.md](../README.md) и [examples.md](examples.md).

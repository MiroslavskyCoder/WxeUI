#pragma once

#include <memory>
#include <unordered_map>
#include <vector>
#include <string>
#include <mutex>
#include <atomic>
#include <chrono>
#include <thread>
#include <queue>
#include <set>

namespace WxeUI {
namespace Cache {

enum class ShaderType {
    VERTEX,         // Вершинный шейдер
    FRAGMENT,       // Фрагментный шейдер
    GEOMETRY,       // Геометрический шейдер
    COMPUTE,        // Вычислительный шейдер
    TESSELLATION_CONTROL,   // Шейдер управления тесселяцией
    TESSELLATION_EVALUATION // Шейдер оценки тесселяции
};

enum class ShaderLanguage {
    HLSL,           // DirectX HLSL
    GLSL,           // OpenGL GLSL
    SPIRV,          // Vulkan SPIR-V
    MSL             // Metal Shading Language
};

enum class OptimizationLevel {
    NONE = 0,       // Без оптимизации
    BASIC = 1,      // Базовая оптимизация
    STANDARD = 2,   // Стандартная оптимизация
    AGGRESSIVE = 3  // Агрессивная оптимизация
};

struct ShaderDefines {
    std::unordered_map<std::string, std::string> defines;
    
    void AddDefine(const std::string& name, const std::string& value = "") {
        defines[name] = value;
    }
    
    void RemoveDefine(const std::string& name) {
        defines.erase(name);
    }
    
    bool HasDefine(const std::string& name) const {
        return defines.find(name) != defines.end();
    }
    
    std::string GetDefineValue(const std::string& name) const {
        auto it = defines.find(name);
        return it != defines.end() ? it->second : "";
    }
    
    // Генерация уникального ключа
    std::string GetCacheKey() const;
    
    bool operator==(const ShaderDefines& other) const {
        return defines == other.defines;
    }
};

struct ShaderDescriptor {
    ShaderType type;
    ShaderLanguage language;
    std::string source_file;        // Путь к файлу с исходным кодом
    std::string entry_point;        // Точка входа (напр. "main")
    std::string target_profile;     // Целевой профиль (напр. "vs_5_0")
    ShaderDefines defines;          // Макросы и определения
    OptimizationLevel optimization; // Уровень оптимизации
    bool debug_info;                // Включать отладочную информацию
    
    ShaderDescriptor() : type(ShaderType::VERTEX), language(ShaderLanguage::HLSL),
                        entry_point("main"), optimization(OptimizationLevel::STANDARD),
                        debug_info(false) {}
    
    // Генерация ключа для кэша
    std::string GetCacheKey() const;
};

struct ShaderBinary {
    std::vector<uint8_t> bytecode;
    size_t size;
    std::string compile_log;        // Лог компиляции
    std::chrono::steady_clock::time_point compile_time;
    std::chrono::steady_clock::time_point last_access;
    uint32_t access_count;
    bool compilation_successful;
    double compile_duration_ms;
    
    // GPU-specific данные
    void* gpu_handle;               // Platform-specific GPU объект
    bool is_resident;               // Находится ли в GPU
    
    ShaderBinary() : size(0), access_count(0), compilation_successful(false),
                    compile_duration_ms(0.0), gpu_handle(nullptr), is_resident(false) {
        compile_time = std::chrono::steady_clock::now();
        last_access = compile_time;
    }
};

struct ShaderProgram {
    std::string name;
    std::unordered_map<ShaderType, std::shared_ptr<ShaderBinary>> shaders;
    void* program_handle;           // Platform-specific program handle
    bool is_linked;
    std::string link_log;
    
    ShaderProgram() : program_handle(nullptr), is_linked(false) {}
    
    bool HasShader(ShaderType type) const {
        return shaders.find(type) != shaders.end();
    }
    
    std::shared_ptr<ShaderBinary> GetShader(ShaderType type) const {
        auto it = shaders.find(type);
        return it != shaders.end() ? it->second : nullptr;
    }
    
    void AddShader(ShaderType type, std::shared_ptr<ShaderBinary> shader) {
        shaders[type] = shader;
    }
};

struct ShaderCacheStats {
    std::atomic<uint64_t> cache_hits{0};
    std::atomic<uint64_t> cache_misses{0};
    std::atomic<uint64_t> compilations{0};
    std::atomic<uint64_t> failed_compilations{0};
    std::atomic<uint64_t> programs_linked{0};
    std::atomic<uint64_t> failed_links{0};
    std::atomic<double> avg_compile_time_ms{0.0};
    std::atomic<size_t> total_bytecode_size{0};
    
    double GetHitRatio() const {
        auto total = cache_hits.load() + cache_misses.load();
        return total > 0 ? static_cast<double>(cache_hits.load()) / total : 0.0;
    }
    
    double GetCompilationSuccessRate() const {
        auto total = compilations.load();
        auto successful = total - failed_compilations.load();
        return total > 0 ? static_cast<double>(successful) / total : 0.0;
    }
};

class ShaderCache {
public:
    struct Config {
        size_t max_cache_size = 256 * 1024 * 1024;      // 256MB общий лимит
        size_t max_entries = 5000;                       // Макс количество шейдеров
        
        // Компиляция
        bool async_compilation = true;                   // Асинхронная компиляция
        uint32_t compiler_threads = 2;                   // Потоки компиляции
        double max_compile_time_ms = 5000.0;             // Макс время компиляции
        
        // Оптимизация
        OptimizationLevel default_optimization = OptimizationLevel::STANDARD;
        bool gpu_specific_optimization = true;           // Оптимизация под GPU
        bool enable_shader_reflection = true;            // Рефлекция шейдеров
        
        // Предзагрузка
        bool enable_precompilation = true;               // Предварительная компиляция
        std::vector<std::string> precompile_shaders;     // Список шейдеров для прекомпиляции
        
        // Кэш на диске
        bool persistent_cache = true;
        std::string cache_directory = "shader_cache";
        bool compress_bytecode = true;                   // Сжимать bytecode
        
        // Очистка
        std::chrono::seconds max_unused_time{600};       // 10 минут
        double cleanup_threshold = 0.9;                  // 90% заполнения
    };
    
    using CompileCallback = std::function<void(const std::string&, bool, const std::string&)>;
    
public:
    explicit ShaderCache(const Config& config = Config{});
    ~ShaderCache();
    
    // Инициализация
    bool Initialize();
    void Shutdown();
    
    // Основные операции
    std::shared_ptr<ShaderBinary> GetShader(const ShaderDescriptor& descriptor);
    std::shared_ptr<ShaderBinary> GetShader(const std::string& cache_key);
    
    bool CompileShader(const ShaderDescriptor& descriptor, 
                      const std::string& source_code,
                      std::shared_ptr<ShaderBinary>& binary);
    
    bool CompileShaderAsync(const ShaderDescriptor& descriptor,
                           const std::string& source_code,
                           CompileCallback callback = nullptr);
    
    // Программы
    std::shared_ptr<ShaderProgram> CreateProgram(const std::string& name);
    std::shared_ptr<ShaderProgram> GetProgram(const std::string& name);
    bool LinkProgram(std::shared_ptr<ShaderProgram> program);
    
    // Загрузка из файлов
    std::shared_ptr<ShaderBinary> LoadShaderFromFile(const std::string& filepath,
                                                     const ShaderDescriptor& descriptor);
    
    bool LoadShaderFromSource(const std::string& source_code,
                             const ShaderDescriptor& descriptor,
                             std::shared_ptr<ShaderBinary>& binary);
    
    // Предкомпиляция
    void PrecompileShaders(const std::vector<ShaderDescriptor>& descriptors);
    bool IsPrecompiling() const;
    void WaitForPrecompilation();
    
    // Кэш управление
    bool RemoveShader(const std::string& cache_key);
    void ClearCache();
    void InvalidateShader(const std::string& source_file); // При изменении файла
    
    // Статистика
    ShaderCacheStats GetStats() const;
    void ResetStats();
    size_t GetCacheSize() const;
    size_t GetEntryCount() const;
    
    // Конфигурация
    void UpdateConfig(const Config& new_config);
    const Config& GetConfig() const { return config_; }
    
    // Утилиты
    static std::string GetShaderTypeName(ShaderType type);
    static std::string GetLanguageName(ShaderLanguage language);
    static bool ValidateShaderSource(const std::string& source, ShaderLanguage language);
    
private:
    Config config_;
    
    // Хранилище шейдеров
    std::unordered_map<std::string, std::shared_ptr<ShaderBinary>> cache_;
    std::unordered_map<std::string, std::shared_ptr<ShaderProgram>> programs_;
    mutable std::shared_mutex cache_mutex_;
    
    // Статистика
    mutable ShaderCacheStats stats_;
    
    // Компиляция
    struct CompileTask {
        ShaderDescriptor descriptor;
        std::string source_code;
        CompileCallback callback;
        std::chrono::steady_clock::time_point submit_time;
    };
    
    std::queue<CompileTask> compile_queue_;
    std::vector<std::thread> compiler_threads_;
    std::atomic<bool> compilation_active_{false};
    std::mutex compile_mutex_;
    std::condition_variable compile_cv_;
    
    // Platform-specific данные
#ifdef _WIN32
    struct Win32ShaderData;
    std::unique_ptr<Win32ShaderData> win32_data_;
#endif
    
    // Внутренние методы
    bool InitializePlatform();
    void ShutdownPlatform();
    
    void CompilerWorker();
    bool CompileShaderInternal(const ShaderDescriptor& descriptor,
                              const std::string& source_code,
                              std::shared_ptr<ShaderBinary>& binary);
    
    // Платформа-специфичная компиляция
    bool CompileHLSL(const ShaderDescriptor& descriptor, const std::string& source,
                    std::vector<uint8_t>& bytecode, std::string& error_log);
    bool CompileGLSL(const ShaderDescriptor& descriptor, const std::string& source,
                    std::vector<uint8_t>& bytecode, std::string& error_log);
    bool CompileSPIRV(const ShaderDescriptor& descriptor, const std::string& source,
                     std::vector<uint8_t>& bytecode, std::string& error_log);
    
    // Оптимизация
    std::vector<uint8_t> OptimizeShader(const std::vector<uint8_t>& bytecode,
                                       const ShaderDescriptor& descriptor);
    
    // Кэш на диске
    bool SaveToFile(const std::string& cache_key, const ShaderBinary& binary);
    bool LoadFromFile(const std::string& cache_key, std::shared_ptr<ShaderBinary>& binary);
    std::string GetCacheFilePath(const std::string& cache_key) const;
    
    // Управление памятью
    void CleanupUnused();
    void EvictLRU();
    bool NeedsEviction() const;
    
    // Утилиты
    void UpdateAccessTime(std::shared_ptr<ShaderBinary> binary);
    void UpdateStats(bool cache_hit, double compile_time = 0.0);
    std::string GenerateCacheKey(const ShaderDescriptor& descriptor) const;
};

} // namespace Cache
} // namespace WindowWinapi

//
// Created by leoni on 14.04.2026.
//

#ifndef TECHNOLOGICALPRACTICE_FILESYSTEM_H
#define TECHNOLOGICALPRACTICE_FILESYSTEM_H

#include <string>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

// Разрешает путь к ресурсу (шейдеру, модели и т.п.) в три приоритета:
//   1. Переменная окружения LOGL_ROOT_PATH (если задана) -- удобно для
//      запуска отладочной сборки из произвольного каталога.
//   2. Каталог, в котором лежит исполняемый файл. POST_BUILD-команда
//      CMake копирует папки `shaders/` и `models/` рядом с .exe, так что
//      relative path "models/foo.obj" разрешается корректно вне зависимости
//      от текущего рабочего каталога.
//   3. Запасной вариант -- текущий рабочий каталог.
class FileSystem
{
public:
    static std::string getPath(const std::string &path)
    {
        return getRoot() + "/" + path;
    }

private:
    static const std::string &getRoot()
    {
        static const std::string root = computeRoot();
        return root;
    }

    static std::string computeRoot()
    {
        if (const char *env = std::getenv("LOGL_ROOT_PATH"))
            return env;
#ifdef _WIN32
        char buf[MAX_PATH];
        DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
        if (n > 0 && n < MAX_PATH)
        {
            std::string s(buf, n);
            size_t pos = s.find_last_of("/\\");
            if (pos != std::string::npos)
                return s.substr(0, pos);
        }
#endif
        return ".";
    }
};


#endif //TECHNOLOGICALPRACTICE_FILESYSTEM_H

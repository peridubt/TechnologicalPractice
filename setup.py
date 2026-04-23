import platform
import subprocess
import os
import sys

win_libs = ["assimp", "opengl", "glfw3", "glad", "glm"]
linux_libs = [
    "build-essential",
    "mesa-common-dev",
    "libgl1-mesa-dev",
    "libglu1-mesa-dev",
    "mesa-utils",
    "libglm-dev",
    "libglfw3-dev",
    "libglew-dev",
    "libassimp-dev",
    "python3-pip",
]

GLAD_OUT_PATH = "glad"
GLAD_API = "gl:core=3.3"


def get_os() -> str:
    return platform.system()


def has_vcpkg() -> bool:
    vcpkg_root = os.environ.get("VCPKG_ROOT")
    return bool(vcpkg_root and os.path.isdir(vcpkg_root))


def load_vcpkg():
    subprocess.run(["git", "clone", "https://github.com/microsoft/vcpkg.git"], check=True)
    subprocess.run([".\\bootstrap-vcpkg.bat"], check=True, cwd="vcpkg", shell=True)
    vcpkg_path = os.path.abspath("vcpkg")
    os.environ["VCPKG_ROOT"] = vcpkg_path


def get_vcpkg_executable() -> str:
    vcpkg_root = os.environ.get("VCPKG_ROOT", "vcpkg")
    return os.path.join(vcpkg_root, "vcpkg.exe")


def has_cmake() -> bool:
    try:
        subprocess.check_output(["cmake", "--version"])
        return True
    except (FileNotFoundError, subprocess.CalledProcessError):
        return False


def configure_cmake_win():
    if not has_cmake():
        print("CMake не найден, установка через winget...")
        subprocess.run(["winget", "install", "Kitware.CMake"], check=True)

    vcpkg_root = os.environ.get("VCPKG_ROOT")
    if not vcpkg_root:
        raise EnvironmentError("VCPKG_ROOT не задан")

    toolchain = os.path.join(vcpkg_root, "scripts", "buildsystems", "vcpkg.cmake")

    triplet = "x64-mingw-dynamic" if has_mingw() else "x64-windows"

    subprocess.run([
        "cmake", "-B", "build", "-S", ".",
        f"-DVCPKG_TARGET_TRIPLET={triplet}",
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
    ], check=True)


def has_mingw() -> bool:
    try:
        subprocess.check_output(["gcc", "--version"])
        return True
    except (FileNotFoundError, subprocess.CalledProcessError):
        return False


def load_libs_on_win():
    if not has_vcpkg():
        load_vcpkg()

    triplet = "x64-mingw-dynamic" if has_mingw() else "x64-windows"
    vcpkg = get_vcpkg_executable()

    for lib in win_libs:
        subprocess.run([vcpkg, "install", f"{lib}:{triplet}"], check=True)

    configure_cmake_win()


def install_glad_linux():
    print("Устанавливается генератор GLAD...")
    subprocess.run([sys.executable, "-m", "pip", "install", "glad2"], check=True)

    if not os.path.exists(GLAD_OUT_PATH):
        os.makedirs(GLAD_OUT_PATH)

    print(f"Генерируется GLAD ({GLAD_API}) в папку '{GLAD_OUT_PATH}'...")
    subprocess.run([
        sys.executable, "-m", "glad",
        "--api", GLAD_API,
        "--out-path", GLAD_OUT_PATH,
    ], check=True)
    print(f"GLAD сгенерирован. Подключите '{GLAD_OUT_PATH}/src/gl.c' к своему проекту.")


def load_libs_on_linux():
    print("Устанавливаются системные пакеты...")
    subprocess.run(["sudo", "apt", "update"], check=True)
    subprocess.run(["sudo", "apt", "install", "-y"] + linux_libs, check=True)

    install_glad_linux()


def load_libs():
    current_os = get_os()
    print(f"Обнаружена ОС: {current_os}")

    match current_os:
        case "Windows":
            load_libs_on_win()
        case "Linux":
            load_libs_on_linux()
        case _:
            print(f"ОС '{current_os}' не поддерживается.")
            sys.exit(1)


if __name__ == "__main__":
    load_libs()
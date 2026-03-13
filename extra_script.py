# extra_script.py — PlatformIO build hook
# Adds the CC310 crypto library path that the nordicnrf52 platform builder misses.
# The Adafruit_nRFCrypto library declares ldflags=-lnrf_cc310_0.9.13-no-interrupts
# but PlatformIO doesn't add the -L path for the precompiled .a file.

Import("env")
import os

framework_dir = env.PioPlatform().get_package_dir("framework-arduinoadafruitnrf52")
crypto_lib_path = os.path.join(
    framework_dir, "libraries", "Adafruit_nRFCrypto",
    "src", "cortex-m4", "fpv4-sp-d16-hard"
)

if os.path.isdir(crypto_lib_path):
    env.Append(LIBPATH=[crypto_lib_path])

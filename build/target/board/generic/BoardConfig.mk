# config.mk
#
# Product-specific compile-time definitions.
#

# The generic product target doesn't have any hardware-specific pieces.
TARGET_NO_BOOTLOADER := true
TARGET_NO_KERNEL := true
TARGET_CPU_ABI := armeabi
HAVE_HTC_AUDIO_DRIVER := false
BOARD_USES_GENERIC_AUDIO := true

# no hardware camera
USE_CAMERA_STUB := false
BOARD_ENABLE_GSTREAMER := true
BOARD_USES_ALSA_AUDIO :=true
BUILD_WITH_ALSA_UTILS := true
BOARD_ENABLE_FAST_OVERLAY := true

#ifndef DISK_TOOL_H
#define DISK_TOOL_H

#include <QString>

class DiskTool {
public:
    // Format a drive (e.g. "E:" on Windows, "/dev/sdb1" on Linux)
    // fs: FAT32/NTFS/EXT4, label: volume label
    static bool formatDrive(const QString& drive, const QString& fs = "FAT32", const QString& label = "USB");

    // Safely eject/unmount a drive
    static bool ejectDrive(const QString& drive);
};

#endif // DISK_TOOL_H

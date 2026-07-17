"""Minimal Windows NT handle-relative primitives for evidence publication."""

from __future__ import annotations

import ctypes
import hashlib
import os
from ctypes import wintypes


if ctypes.sizeof(ctypes.c_void_p) != 8:
    raise OSError("64-bit Windows is required")

ntdll = ctypes.WinDLL("ntdll", use_last_error=True)
kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)

OBJ_CASE_INSENSITIVE = 0x40
OBJ_DONT_REPARSE = 0x1000
FILE_CREATE = 2
FILE_OPEN = 1
FILE_DIRECTORY_FILE = 0x00000001
FILE_NON_DIRECTORY_FILE = 0x00000040
FILE_SYNCHRONOUS_IO_NONALERT = 0x00000020
FILE_OPEN_REPARSE_POINT = 0x00200000
FILE_ATTRIBUTE_NORMAL = 0x80
FILE_ATTRIBUTE_REPARSE_POINT = 0x400
FILE_SHARE_READ_WRITE = 0x3
SYNCHRONIZE = 0x00100000
DELETE = 0x00010000
FILE_LIST_DIRECTORY = 0x0001
FILE_ADD_FILE = 0x0002
FILE_ADD_SUBDIRECTORY = 0x0004
FILE_READ_ATTRIBUTES = 0x0080
FILE_WRITE_DATA = 0x0002
FILE_WRITE_ATTRIBUTES = 0x0100
FILE_GENERIC_WRITE = 0x00120116
OPEN_EXISTING = 3
FILE_FLAG_BACKUP_SEMANTICS = 0x02000000
FILE_FLAG_OPEN_REPARSE_POINT = 0x00200000
INVALID_HANDLE_VALUE = ctypes.c_void_p(-1).value
FileRenameInfo = 3


class UNICODE_STRING(ctypes.Structure):
    _fields_ = [("Length", wintypes.USHORT), ("MaximumLength", wintypes.USHORT), ("Buffer", wintypes.LPWSTR)]


class OBJECT_ATTRIBUTES(ctypes.Structure):
    _fields_ = [
        ("Length", wintypes.ULONG), ("RootDirectory", wintypes.HANDLE),
        ("ObjectName", ctypes.POINTER(UNICODE_STRING)), ("Attributes", wintypes.ULONG),
        ("SecurityDescriptor", ctypes.c_void_p), ("SecurityQualityOfService", ctypes.c_void_p),
    ]


class IO_STATUS_BLOCK(ctypes.Structure):
    _fields_ = [("Status", ctypes.c_void_p), ("Information", ctypes.c_void_p)]


class BY_HANDLE_FILE_INFORMATION(ctypes.Structure):
    _fields_ = [
        ("dwFileAttributes", wintypes.DWORD), ("ftCreationTime", wintypes.FILETIME),
        ("ftLastAccessTime", wintypes.FILETIME), ("ftLastWriteTime", wintypes.FILETIME),
        ("dwVolumeSerialNumber", wintypes.DWORD), ("nFileSizeHigh", wintypes.DWORD),
        ("nFileSizeLow", wintypes.DWORD), ("nNumberOfLinks", wintypes.DWORD),
        ("nFileIndexHigh", wintypes.DWORD), ("nFileIndexLow", wintypes.DWORD),
    ]


ntdll.NtCreateFile.restype = wintypes.LONG
kernel32.CreateFileW.restype = wintypes.HANDLE
kernel32.SetFileInformationByHandle.restype = wintypes.BOOL
kernel32.WriteFile.restype = wintypes.BOOL
kernel32.FlushFileBuffers.restype = wintypes.BOOL
kernel32.GetFileInformationByHandle.restype = wintypes.BOOL
kernel32.CloseHandle.restype = wintypes.BOOL
ntdll.NtSetInformationFile.restype = wintypes.LONG


def _raise_win32(label: str) -> None:
    raise OSError(ctypes.get_last_error(), label)


def close(handle: int | None) -> None:
    if handle and handle != INVALID_HANDLE_VALUE:
        kernel32.CloseHandle(wintypes.HANDLE(handle))


def open_directory(path: str, writable: bool = True) -> int:
    access = SYNCHRONIZE | FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES
    if writable:
        access |= FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY
    handle = kernel32.CreateFileW(path, access, FILE_SHARE_READ_WRITE, None, OPEN_EXISTING,
                                  FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT, None)
    if handle == INVALID_HANDLE_VALUE:
        _raise_win32("CreateFileW directory")
    info = BY_HANDLE_FILE_INFORMATION()
    if not kernel32.GetFileInformationByHandle(handle, ctypes.byref(info)):
        close(handle); _raise_win32("GetFileInformationByHandle")
    if info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT:
        close(handle); raise OSError("directory_reparse_rejected")
    return int(handle)


def open_child_directory(parent: int, name: str, writable: bool = True) -> int:
    if not name or name in (".", "..") or any(c in name for c in "\\/:\0"):
        raise OSError("relative_directory_leaf_rejected")
    buffer = ctypes.create_unicode_buffer(name); encoded_bytes = len(name.encode("utf-16-le"))
    unicode = UNICODE_STRING(encoded_bytes, encoded_bytes + 2, ctypes.cast(buffer, wintypes.LPWSTR))
    attributes = OBJECT_ATTRIBUTES(ctypes.sizeof(OBJECT_ATTRIBUTES), wintypes.HANDLE(parent),
                                   ctypes.pointer(unicode), OBJ_CASE_INSENSITIVE | OBJ_DONT_REPARSE, None, None)
    iosb = IO_STATUS_BLOCK(); handle = wintypes.HANDLE()
    desired = SYNCHRONIZE | FILE_LIST_DIRECTORY | FILE_READ_ATTRIBUTES
    if writable:
        desired |= FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY
    status = ntdll.NtCreateFile(ctypes.byref(handle), desired, ctypes.byref(attributes), ctypes.byref(iosb),
                                None, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ_WRITE, FILE_OPEN,
                                FILE_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_REPARSE_POINT,
                                None, 0)
    if status < 0:
        raise OSError(f"NtCreateFile directory failed:0x{status & 0xffffffff:08x}")
    info = BY_HANDLE_FILE_INFORMATION()
    if not kernel32.GetFileInformationByHandle(handle, ctypes.byref(info)):
        close(int(handle.value)); _raise_win32("GetFileInformationByHandle child")
    if info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT:
        close(int(handle.value)); raise OSError("directory_reparse_rejected")
    return int(handle.value)


def open_directory_tree(path: str, writable_final: bool = True) -> int:
    absolute = os.path.abspath(path)
    drive, tail = os.path.splitdrive(absolute)
    if not drive or drive.startswith("\\") or ":" in tail or absolute.startswith(("\\\\.\\", "\\\\?\\")):
        raise OSError("windows_device_or_unc_path_rejected")
    handle = open_directory(drive + "\\", writable=False)
    try:
        components = [value for value in tail.replace("/", "\\").split("\\") if value]
        for index, component in enumerate(components):
            child = open_child_directory(handle, component, writable=writable_final and index == len(components) - 1)
            close(handle)
            handle = child
        return handle
    except Exception:
        close(handle)
        raise


def create_relative(parent: int, name: str, *, directory: bool) -> int:
    if not name or name in (".", "..") or any(c in name for c in "\\/:\0"):
        raise OSError("relative_leaf_rejected")
    buffer = ctypes.create_unicode_buffer(name)
    encoded_bytes = len(name.encode("utf-16-le"))
    unicode = UNICODE_STRING(encoded_bytes, encoded_bytes + 2, ctypes.cast(buffer, wintypes.LPWSTR))
    attributes = OBJECT_ATTRIBUTES(ctypes.sizeof(OBJECT_ATTRIBUTES), wintypes.HANDLE(parent),
                                   ctypes.pointer(unicode), OBJ_CASE_INSENSITIVE | OBJ_DONT_REPARSE, None, None)
    iosb = IO_STATUS_BLOCK()
    handle = wintypes.HANDLE()
    options = FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_REPARSE_POINT
    desired = SYNCHRONIZE | DELETE | FILE_READ_ATTRIBUTES
    if directory:
        options |= FILE_DIRECTORY_FILE
        desired |= FILE_LIST_DIRECTORY | FILE_ADD_FILE | FILE_ADD_SUBDIRECTORY
    else:
        options |= FILE_NON_DIRECTORY_FILE
        desired |= FILE_GENERIC_WRITE | FILE_WRITE_DATA | FILE_WRITE_ATTRIBUTES
    status = ntdll.NtCreateFile(ctypes.byref(handle), desired, ctypes.byref(attributes), ctypes.byref(iosb),
                                None, FILE_ATTRIBUTE_NORMAL, FILE_SHARE_READ_WRITE, FILE_CREATE, options, None, 0)
    if status < 0:
        raise FileExistsError(f"NtCreateFile failed:0x{status & 0xffffffff:08x}")
    return int(handle.value)


def open_relative(parent: int, name: str, maximum: int) -> bytes:
    if not name or any(c in name for c in "\\/:\0"):
        raise OSError("relative_leaf_rejected")
    buffer = ctypes.create_unicode_buffer(name)
    encoded_bytes = len(name.encode("utf-16-le"))
    unicode = UNICODE_STRING(encoded_bytes, encoded_bytes + 2, ctypes.cast(buffer, wintypes.LPWSTR))
    attributes = OBJECT_ATTRIBUTES(ctypes.sizeof(OBJECT_ATTRIBUTES), wintypes.HANDLE(parent),
                                   ctypes.pointer(unicode), OBJ_CASE_INSENSITIVE | OBJ_DONT_REPARSE, None, None)
    iosb = IO_STATUS_BLOCK(); handle = wintypes.HANDLE()
    status = ntdll.NtCreateFile(ctypes.byref(handle), 0x80000000 | SYNCHRONIZE | FILE_READ_ATTRIBUTES,
                                ctypes.byref(attributes), ctypes.byref(iosb), None, FILE_ATTRIBUTE_NORMAL,
                                1, FILE_OPEN, FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT |
                                FILE_OPEN_REPARSE_POINT, None, 0)
    if status < 0:
        raise OSError(f"NtCreateFile open failed:0x{status & 0xffffffff:08x}")
    try:
        size = ctypes.c_longlong()
        if not kernel32.GetFileSizeEx(handle, ctypes.byref(size)):
            _raise_win32("GetFileSizeEx")
        if size.value < 0 or size.value > maximum:
            raise OSError("opened_file_size_rejected")
        result = bytearray()
        remaining = size.value
        while remaining:
            count = min(1024 * 1024, remaining)
            data = ctypes.create_string_buffer(count); read = wintypes.DWORD()
            if not kernel32.ReadFile(handle, data, count, ctypes.byref(read), None):
                _raise_win32("ReadFile")
            if read.value == 0:
                raise OSError("opened_file_short_read")
            result.extend(data.raw[:read.value]); remaining -= read.value
        return bytes(result)
    finally:
        close(int(handle.value))


def open_relative_handle(parent: int, name: str) -> int:
    """Return one held regular leaf handle with write/delete sharing denied."""
    if not name or any(c in name for c in "\\/:\0"):
        raise OSError("relative_leaf_rejected")
    buffer = ctypes.create_unicode_buffer(name); encoded_bytes = len(name.encode("utf-16-le"))
    unicode = UNICODE_STRING(encoded_bytes, encoded_bytes + 2, ctypes.cast(buffer, wintypes.LPWSTR))
    attributes = OBJECT_ATTRIBUTES(ctypes.sizeof(OBJECT_ATTRIBUTES), wintypes.HANDLE(parent),
                                   ctypes.pointer(unicode), OBJ_CASE_INSENSITIVE | OBJ_DONT_REPARSE, None, None)
    iosb = IO_STATUS_BLOCK(); handle = wintypes.HANDLE()
    status = ntdll.NtCreateFile(ctypes.byref(handle), 0x80000000 | SYNCHRONIZE | FILE_READ_ATTRIBUTES,
                                ctypes.byref(attributes), ctypes.byref(iosb), None, FILE_ATTRIBUTE_NORMAL,
                                1, FILE_OPEN, FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT |
                                FILE_OPEN_REPARSE_POINT, None, 0)
    if status < 0:
        raise OSError(f"NtCreateFile open handle failed:0x{status & 0xffffffff:08x}")
    info = BY_HANDLE_FILE_INFORMATION()
    if not kernel32.GetFileInformationByHandle(handle, ctypes.byref(info)):
        close(int(handle.value)); _raise_win32("GetFileInformationByHandle leaf")
    if info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT or info.nNumberOfLinks != 1:
        close(int(handle.value)); raise OSError("leaf_reparse_or_link_rejected")
    return int(handle.value)


def hash_relative(parent: int, name: str) -> tuple[str, int, bytes]:
    if not name or any(c in name for c in "\\/:\0"):
        raise OSError("relative_leaf_rejected")
    buffer = ctypes.create_unicode_buffer(name); encoded_bytes = len(name.encode("utf-16-le"))
    unicode = UNICODE_STRING(encoded_bytes, encoded_bytes + 2, ctypes.cast(buffer, wintypes.LPWSTR))
    attributes = OBJECT_ATTRIBUTES(ctypes.sizeof(OBJECT_ATTRIBUTES), wintypes.HANDLE(parent),
                                   ctypes.pointer(unicode), OBJ_CASE_INSENSITIVE | OBJ_DONT_REPARSE, None, None)
    iosb = IO_STATUS_BLOCK(); handle = wintypes.HANDLE()
    status = ntdll.NtCreateFile(ctypes.byref(handle), 0x80000000 | SYNCHRONIZE | FILE_READ_ATTRIBUTES,
                                ctypes.byref(attributes), ctypes.byref(iosb), None, FILE_ATTRIBUTE_NORMAL,
                                1, FILE_OPEN, FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT |
                                FILE_OPEN_REPARSE_POINT, None, 0)
    if status < 0: raise OSError(f"NtCreateFile hash failed:0x{status & 0xffffffff:08x}")
    try:
        info = BY_HANDLE_FILE_INFORMATION()
        if not kernel32.GetFileInformationByHandle(handle, ctypes.byref(info)):
            _raise_win32("GetFileInformationByHandle hash")
        if info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT or info.nNumberOfLinks != 1:
            raise OSError("hashed_leaf_reparse_or_link_rejected")
        size = ctypes.c_longlong()
        if not kernel32.GetFileSizeEx(handle, ctypes.byref(size)): _raise_win32("GetFileSizeEx")
        digest = hashlib.sha256(); total = 0; prefix = b""
        while total < size.value:
            count = min(8 * 1024 * 1024, size.value - total)
            data = ctypes.create_string_buffer(count); read = wintypes.DWORD()
            if not kernel32.ReadFile(handle, data, count, ctypes.byref(read), None): _raise_win32("ReadFile")
            if read.value == 0: raise OSError("opened_file_short_read")
            chunk = data.raw[:read.value]
            if len(prefix) < 4: prefix += chunk[:4 - len(prefix)]
            digest.update(chunk); total += read.value
        return digest.hexdigest(), total, prefix
    finally:
        close(int(handle.value))


def list_directory(handle: int) -> set[str]:
    result: set[str] = set()
    information_class = 11
    while True:
        buffer = ctypes.create_string_buffer(64 * 1024)
        if not kernel32.GetFileInformationByHandleEx(wintypes.HANDLE(handle), information_class, buffer, len(buffer)):
            error = ctypes.get_last_error()
            if error == 18:
                break
            raise OSError(error, "GetFileInformationByHandleEx directory")
        information_class = 10
        offset = 0
        while True:
            next_offset = int.from_bytes(buffer.raw[offset:offset + 4], "little")
            name_length = int.from_bytes(buffer.raw[offset + 60:offset + 64], "little")
            name = buffer.raw[offset + 104:offset + 104 + name_length].decode("utf-16-le")
            if name not in (".", ".."):
                result.add(name)
            if next_offset == 0:
                break
            offset += next_offset
        if next_offset == 0:
            break
    return result


def write_all(handle: int, data: bytes) -> None:
    offset = 0
    while offset < len(data):
        chunk = data[offset:offset + 1024 * 1024]
        written = wintypes.DWORD()
        buffer = ctypes.create_string_buffer(chunk)
        if not kernel32.WriteFile(wintypes.HANDLE(handle), buffer, len(chunk), ctypes.byref(written), None):
            _raise_win32("WriteFile")
        if written.value == 0:
            raise OSError("WriteFile short write")
        offset += written.value
    if not kernel32.FlushFileBuffers(wintypes.HANDLE(handle)):
        _raise_win32("FlushFileBuffers")


def flush(handle: int) -> None:
    if not kernel32.FlushFileBuffers(wintypes.HANDLE(handle)):
        _raise_win32("FlushFileBuffers directory")


def rename_relative(handle: int, destination_root: int, name: str) -> None:
    if not name or any(c in name for c in "\\/:\0"):
        raise OSError("rename_leaf_rejected")
    class FILE_RENAME_INFO(ctypes.Structure):
        _fields_ = [
            ("ReplaceIfExists", wintypes.BOOL), ("RootDirectory", wintypes.HANDLE),
            ("FileNameLength", wintypes.DWORD), ("FileName", wintypes.WCHAR * len(name)),
        ]
    info = FILE_RENAME_INFO()
    info.ReplaceIfExists = False
    info.RootDirectory = wintypes.HANDLE(destination_root)
    info.FileNameLength = len(name.encode("utf-16-le"))
    info.FileName = name
    iosb = IO_STATUS_BLOCK()
    status = ntdll.NtSetInformationFile(wintypes.HANDLE(handle), ctypes.byref(iosb),
                                        ctypes.byref(info), ctypes.sizeof(info), 10)
    if status < 0:
        if status & 0xffffffff == 0xC0000035:
            raise FileExistsError("rename target exists")
        raise OSError(f"NtSetInformationFile rename failed:0x{status & 0xffffffff:08x}")

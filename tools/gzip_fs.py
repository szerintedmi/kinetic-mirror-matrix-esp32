import os
import gzip
import shutil
from typing import Any, TYPE_CHECKING

if TYPE_CHECKING:

    def Import(*args: Any, **kwargs: Any) -> None: ...

    env: Any

Import("env")


def prepare_www_files(target, source, env):
    # Extensions (without dot) to gzip. Everything else gets copied as-is.
    filetypes_to_gzip = {
        "html",
        "css",
        "js",
        "svg",
        "json",
        "xml",
        "txt",
        "ico",
        "wasm",
    }

    print("*** pre:tools/gzip_fs.py COPY/GZIP DATA FILES")

    data_dir = env.get("PROJECT_DATA_DIR")
    project_dir = env.get("PROJECT_DIR")
    data_src_dir = os.path.join(project_dir, "data_src")

    if os.path.exists(data_dir) and not os.path.exists(data_src_dir):
        print(
            f'  "data" dir exists, "data_src" not found → renaming "{data_dir}" → "{data_src_dir}"'
        )
        os.rename(data_dir, data_src_dir)

    # Recreate data dir
    if os.path.exists(data_dir):
        print(f"  Deleting data dir {data_dir}")
        shutil.rmtree(data_dir)
    os.makedirs(data_dir, exist_ok=True)
    print(f"  Re-created empty data dir {data_dir}")

    if not os.path.exists(data_src_dir):
        print(f'  NOTE: "{data_src_dir}" does not exist; nothing to copy/gzip.')
        print("**** END [/COPY/GZIP DATA FILES]")
        return

    # Walk data_src recursively and mirror structure in data
    for root, dirs, files in os.walk(data_src_dir):
        rel_root = os.path.relpath(root, data_src_dir)
        dest_root = data_dir if rel_root == "." else os.path.join(data_dir, rel_root)
        os.makedirs(dest_root, exist_ok=True)

        for name in files:
            src_path = os.path.join(root, name)
            base, ext = os.path.splitext(name)
            ext = ext.lstrip(".").lower()

            if ext in filetypes_to_gzip:
                out_name = f"{name}.gz"  # keep original name + .gz
                out_path = os.path.join(dest_root, out_name)
                print(f"  GZipping: {src_path} → {out_path}")
                # mtime=0 makes gzip output deterministic; drop it if you want real mtimes
                with open(src_path, "rb") as src, gzip.GzipFile(
                    out_path, "wb", mtime=0
                ) as gz:
                    shutil.copyfileobj(src, gz)
            else:
                dst_path = os.path.join(dest_root, name)
                print(f"  Copying:  {src_path} → {dst_path}")
                shutil.copy2(src_path, dst_path)

    print("**** END [/COPY/GZIP DATA FILES]")


# Hook into PlatformIO build Filesystem Image step
env.AddPreAction("buildfs", prepare_www_files)  # type: ignore  # noqa: F821

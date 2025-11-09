Import('env')
import os
import shlex
import subprocess

def run_clang_format(target, source, env):
    project_dir = env['PROJECT_DIR']
    git_cmd = ['git', 'ls-files', '-z', '*.c', '*.cc', '*.cpp', '*.cxx', '*.h', '*.hpp', '*.hxx']
    try:
        files_bytes = subprocess.check_output(git_cmd, cwd=project_dir)
    except subprocess.CalledProcessError as exc:
        env.Exit('git ls-files failed: {}'.format(exc))
    files = [path for path in files_bytes.split(b'\0') if path]
    if not files:
        return
    fix_env = os.environ.get('CLANG_FORMAT_FIX', '').lower()
    fix_mode = fix_env in ('1', 'true', 'yes')
    clang_cmd = ['clang-format', '--style=file']
    if fix_mode:
        clang_cmd.append('-i')
    else:
        clang_cmd += ['--dry-run', '--Werror']
    clang_cmd += [path.decode('utf-8') for path in files]
    try:
        subprocess.check_call(clang_cmd, cwd=project_dir)
    except FileNotFoundError:
        env.Exit('clang-format is not installed. Install it and rerun pio check.')
    except subprocess.CalledProcessError as exc:
        env.Exit('clang-format failed: exit {}' .format(exc.returncode))

env.AddPreAction('check', run_clang_format)

from pathlib import Path

absent_files = set()
directory = Path(aller_dir)
diff_aller_retour_set = set(diff_aller_retour)  # Convert list to set for faster lookup

for file_path in directory.iterdir():
    if file_path.is_file():
        with open(file_path, 'r') as sftrFile:
            content = sftrFile.read()
            if any(BizMsgIdr in content for BizMsgIdr in diff_aller_retour_set):
                print(file_path, BizMsgIdr)
                absent_files.add(str(file_path))
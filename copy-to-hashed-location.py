#!/usr/bin/env python

import shutil
import os
from pathlib import Path


def copy_and_add_hash(target_directory, source, files_to_modify):
	import hashlib
	hash_md5 = hashlib.md5()
	length = 0
	with open(source, "rb") as f:
		for chunk in iter(lambda: f.read(4096), b""):
			length += len(chunk)
			hash_md5.update(chunk)
	hash = hash_md5.hexdigest()
	if '.' in os.path.basename(source):
		base, extensions = os.path.basename(source).split('.', 1)
		extensions = '.' + extensions
	else:
		base = source
		extensions = ''
	dest = base + '-' + hash + extensions
	copy_dest = os.path.join(target, dest)
	print(source, '->', dest)

	total_count = 0
	for to_modify in files_to_modify:
		with open(to_modify, "r+") as f:
			data = f.read()
			f.seek(0)
			count = data.count(source)
			if count:
				print("replacing", count, "occurrence" + ('' if count == 1 else 's'), "of", source, 'in', to_modify)
			output = data.replace(source, dest)
			f.write(output)
			f.truncate()
		total_count += count
	if total_count == 0:
		raise ValueError(repr(files_to_modify) + " do not contain source '"+source+"' to modify!")

	shutil.copy(source, copy_dest)

if __name__ == '__main__':
	FILES_TO_MODIFY = [
		"to-be-modified-endless-sky.html",
		"to-be-modified-endless-sky.js"
	]

	import sys
	target = sys.argv[-1]
	if target[-1] == '/':
		target = target[:-1]
	sources = sys.argv[1:-1]
	if not os.path.isdir(target):
		raise ValueError("dest must be a directory")
	for file_to_modify in FILES_TO_MODIFY:
		if not os.path.exists(file_to_modify):
			raise ValueError(file_to_modify + " must exist to be modified")
	for source in sources:
		copy_and_add_hash(target, source, FILES_TO_MODIFY)

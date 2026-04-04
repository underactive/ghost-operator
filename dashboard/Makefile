.PHONY: install test

install:
	npm install

test:
	node_modules/.bin/vitest run src/lib/dfu/zip.test.js

install-and-test:
	npm install && node_modules/.bin/vitest run src/lib/dfu/zip.test.js

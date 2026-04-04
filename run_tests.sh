#!/bin/sh
cd /private/var/folders/4r/x_8dkg9544x02bt_5qzgk_nh0000gq/T/kaicho-wt-24569/kaicho-fix-4fff6041/dashboard
npm install 2>&1
node_modules/.bin/vitest run src/lib/dfu/zip.test.js 2>&1

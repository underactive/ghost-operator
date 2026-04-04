#!/usr/bin/env node
'use strict';
const { spawnSync } = require('child_process');
const path = require('path');

const dashboardDir = '/private/var/folders/4r/x_8dkg9544x02bt_5qzgk_nh0000gq/T/kaicho-wt-24569/kaicho-fix-4fff6041/dashboard';

// Install deps
console.log('=== npm install ===');
const install = spawnSync('npm', ['install'], {
  cwd: dashboardDir,
  stdio: 'inherit',
  shell: false,
  env: process.env,
});
if (install.status !== 0) {
  console.error('npm install failed with status:', install.status);
  if (install.error) console.error(install.error);
  process.exit(install.status || 1);
}

// Run tests
console.log('\n=== vitest run ===');
const test = spawnSync(
  path.join(dashboardDir, 'node_modules/.bin/vitest'),
  ['run', 'src/lib/dfu/zip.test.js'],
  {
    cwd: dashboardDir,
    stdio: 'inherit',
    shell: false,
    env: process.env,
  }
);
if (test.error) {
  console.error('vitest error:', test.error);
  process.exit(1);
}
process.exit(test.status || 0);

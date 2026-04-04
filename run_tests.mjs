import { execSync } from 'child_process'

const dashboardDir = '/private/var/folders/4r/x_8dkg9544x02bt_5qzgk_nh0000gq/T/kaicho-wt-24569/kaicho-fix-4fff6041/dashboard'

try {
  console.log('=== Installing dependencies ===')
  const installOutput = execSync('npm install', {
    cwd: dashboardDir,
    encoding: 'utf8',
    shell: '/bin/sh',
    stdio: ['pipe', 'pipe', 'pipe']
  })
  console.log(installOutput)
} catch (e) {
  console.log('npm install output:', e.stdout)
  console.log('npm install stderr:', e.stderr)
}

try {
  console.log('=== Running tests ===')
  const testOutput = execSync('node_modules/.bin/vitest run src/lib/dfu/zip.test.js', {
    cwd: dashboardDir,
    encoding: 'utf8',
    shell: '/bin/sh',
    stdio: ['pipe', 'pipe', 'pipe']
  })
  console.log(testOutput)
} catch (e) {
  console.log('Test stdout:', e.stdout)
  console.log('Test stderr:', e.stderr)
  process.exit(e.status || 1)
}

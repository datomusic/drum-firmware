#!/usr/bin/env node

import { spawn } from 'child_process';
import * as fs from 'fs';
import * as path from 'path';
import * as https from 'https';

declare const __filename: string;
const __dirname = path.dirname(__filename);

interface LogEntry {
  level: 'info' | 'warn' | 'error' | 'step';
  message: string;
  timestamp: string;
}

class Logger {
  private readonly useJson: boolean;

  constructor(useJson: boolean) {
    this.useJson = useJson;
  }

  info(message: string): void {
    this.emit('info', message);
  }

  warn(message: string): void {
    this.emit('warn', message);
  }

  step(message: string): void {
    this.emit('step', message);
  }

  error(message: string): void {
    this.emit('error', message);
  }

  flush(result: { success: boolean; error?: string }): void {
    if (this.useJson) {
      const summary = {
        type: 'summary' as const,
        timestamp: new Date().toISOString(),
        ...result,
      };
      process.stdout.write(`${JSON.stringify(summary)}\n`);
    }
  }

  private emit(level: LogEntry['level'], message: string): void {
    const entry: LogEntry = {
      level,
      message,
      timestamp: new Date().toISOString(),
    };

    if (!this.useJson) {
      if (level === 'error') {
        console.error(message);
      } else if (level === 'warn') {
        console.warn(message);
      } else {
        console.log(message);
      }
      return;
    }

    const payload = { type: 'log' as const, ...entry };
    process.stdout.write(`${JSON.stringify(payload)}\n`);
  }
}

interface CommandOptions {
  cwd?: string;
  input?: string;
  silent?: boolean;
  ignoreFailure?: boolean;
}

interface CommandResult {
  stdout: string;
  stderr: string;
  exitCode: number;
}

class CommandError extends Error {
  readonly result: CommandResult;

  constructor(message: string, result: CommandResult) {
    super(message);
    this.result = result;
  }
}

function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

async function runCommand(
  command: string,
  args: string[],
  logger: Logger,
  options: CommandOptions = {},
): Promise<CommandResult> {
  return new Promise((resolve, reject) => {
    const child = spawn(command, args, {
      cwd: options.cwd,
      stdio: 'pipe',
      env: process.env,
    });

    let stdout = '';
    let stderr = '';

    child.stdout.on('data', (data) => {
      stdout += data.toString();
    });

    child.stderr.on('data', (data) => {
      stderr += data.toString();
    });

    child.on('error', (error) => {
      reject(error);
    });

    child.on('close', (code, signal) => {
      const exitCode = code ?? (signal ? 1 : 0);
      const result: CommandResult = { stdout, stderr, exitCode };
      if (exitCode !== 0 && !options.ignoreFailure) {
        reject(new CommandError(`${command} exited with code ${exitCode}`, result));
        return;
      }
      if (!options.silent) {
        if (stdout.trim().length > 0) {
          logger.info(stdout.trimEnd());
        }
        if (stderr.trim().length > 0) {
          logger.warn(stderr.trimEnd());
        }
      }
      resolve(result);
    });

    if (options.input) {
      child.stdin.write(options.input);
    }
    child.stdin.end();
  });
}

async function waitForBootsel(timeoutSeconds: number, logger: Logger): Promise<void> {
  const start = Date.now();

  while (true) {
    const result = await runCommand('picotool', ['info'], logger, { silent: true, ignoreFailure: true });
    if (result.exitCode === 0) {
      return;
    }

    const elapsedSeconds = (Date.now() - start) / 1000;
    if (elapsedSeconds >= timeoutSeconds) {
      throw new Error(`Timed out after ${timeoutSeconds}s waiting for BOOTSEL mode.`);
    }

    await sleep(1000);
  }
}

async function waitForVolume(volumeName: string, timeoutSeconds: number, logger: Logger): Promise<boolean> {
  const volumePath = path.join('/Volumes', volumeName);
  const start = Date.now();

  while (true) {
    if (fs.existsSync(volumePath)) {
      return true;
    }

    const elapsedSeconds = (Date.now() - start) / 1000;
    if (elapsedSeconds >= timeoutSeconds) {
      logger.warn(`Volume '${volumeName}' not found after ${timeoutSeconds}s (platform-specific mount paths may vary).`);
      return false;
    }

    await sleep(1000);
  }
}

async function waitForDevice(timeoutSeconds: number, logger: Logger): Promise<void> {
  const start = Date.now();

  while (true) {
    const result = await runCommand('picotool', ['info'], logger, { silent: true, ignoreFailure: true });
    if (result.exitCode === 0) {
      return;
    }

    const elapsedSeconds = (Date.now() - start) / 1000;
    if (elapsedSeconds >= timeoutSeconds) {
      throw new Error(`Timed out after ${timeoutSeconds}s waiting for device to be ready.`);
    }

    await sleep(1000);
  }
}

interface GitHubAsset {
  name: string;
  browser_download_url: string;
  size: number;
}

interface GitHubRelease {
  tag_name: string;
  name: string;
  assets: GitHubAsset[];
}

async function fetchLatestRelease(logger: Logger): Promise<GitHubRelease> {
  return new Promise((resolve, reject) => {
    const options = {
      hostname: 'api.github.com',
      path: '/repos/datomusic/drum-firmware/releases/latest',
      method: 'GET',
      headers: {
        'User-Agent': 'drum-provision-tool',
        'Accept': 'application/vnd.github.v3+json'
      }
    };

    const req = https.request(options, (res) => {
      let data = '';

      res.on('data', (chunk) => {
        data += chunk;
      });

      res.on('end', () => {
        if (res.statusCode !== 200) {
          reject(new Error(`GitHub API request failed with status ${res.statusCode}: ${data}`));
          return;
        }

        try {
          const release = JSON.parse(data) as GitHubRelease;
          resolve(release);
        } catch (error) {
          reject(new Error(`Failed to parse GitHub API response: ${error instanceof Error ? error.message : 'Unknown error'}`));
        }
      });
    });

    req.on('error', (error) => {
      reject(new Error(`GitHub API request failed: ${error.message}`));
    });

    req.setTimeout(30000, () => {
      req.destroy();
      reject(new Error('GitHub API request timed out'));
    });

    req.end();
  });
}

function findFirmwareAsset(assets: GitHubAsset[], logger: Logger): GitHubAsset | null {
  const semverPattern = /^drum-v?\d+\.\d+\.\d+\.uf2$/;
  const drumPattern = /^drum.*\.uf2$/;

  let semverAsset = assets.find(asset => semverPattern.test(asset.name));
  if (semverAsset) {
    return semverAsset;
  }

  let drumAsset = assets.find(asset => drumPattern.test(asset.name));
  if (drumAsset) {
    return drumAsset;
  }

  return null;
}

async function downloadFile(url: string, destination: string, logger: Logger): Promise<void> {
  return new Promise((resolve, reject) => {
    const file = fs.createWriteStream(destination);

    function makeRequest(requestUrl: string, redirectCount = 0) {
      if (redirectCount > 5) {
        fs.unlinkSync(destination);
        reject(new Error('Too many redirects'));
        return;
      }

      https.get(requestUrl, (response) => {
        if (response.statusCode === 302 || response.statusCode === 301) {
          const newUrl = response.headers.location;
          if (!newUrl) {
            fs.unlinkSync(destination);
            reject(new Error('Redirect without location header'));
            return;
          }
          response.destroy();
          makeRequest(newUrl, redirectCount + 1);
          return;
        }

        if (response.statusCode !== 200) {
          fs.unlinkSync(destination);
          reject(new Error(`Download failed with status ${response.statusCode}`));
          return;
        }

        response.pipe(file);

        file.on('finish', () => {
          file.close();
          resolve();
        });
      }).on('error', (error) => {
        fs.unlinkSync(destination);
        reject(error);
      });
    }

    makeRequest(url);
  });
}

async function downloadLatestFirmware(scriptDir: string, logger: Logger): Promise<string | null> {
  try {
    const release = await fetchLatestRelease(logger);
    const asset = findFirmwareAsset(release.assets, logger);
    if (!asset) {
      logger.warn('⚠ No firmware assets found in latest release');
      return null;
    }

    const cacheDir = path.join(scriptDir, '.cache');
    if (!fs.existsSync(cacheDir)) {
      fs.mkdirSync(cacheDir, { recursive: true });
    }

    const cachedPath = path.join(cacheDir, `${release.tag_name}-${asset.name}`);

    if (fs.existsSync(cachedPath)) {
      logger.info(`✓ Using cached firmware: ${release.tag_name}`);
      return cachedPath;
    }

    logger.info(`Downloading firmware: ${release.tag_name} (${(asset.size / 1024 / 1024).toFixed(1)}MB)...`);
    await downloadFile(asset.browser_download_url, cachedPath, logger);
    logger.info('✓ Download complete');

    const localPath = path.join(scriptDir, 'drum.uf2');
    if (fs.existsSync(localPath)) {
      fs.unlinkSync(localPath);
    }
    fs.copyFileSync(cachedPath, localPath);

    return cachedPath;
  } catch (error) {
    logger.warn(`⚠ Download failed: ${error instanceof Error ? error.message : 'Unknown error'}`);
    return null;
  }
}

async function hasRequiredPartitions(logger: Logger): Promise<boolean> {
  try {
    const result = await runCommand('picotool', ['partition', 'info', '-f'], logger, {
      silent: true,
      ignoreFailure: true
    });

    if (result.exitCode !== 0) {
      return false;
    }

    const output = result.stdout;
    const hasPartition0A = /0\(A\)[\s\S]*?"Firmware A"/.test(output);
    const hasPartition1B = /1\(B w\/ 0\)[\s\S]*?"Firmware B"/.test(output);
    const hasPartition2A = /2\(A\)[\s\S]*?"Data"/.test(output);

    return hasPartition0A && hasPartition1B && hasPartition2A;
  } catch (error) {
    logger.warn(`Failed to check partition info: ${error instanceof Error ? error.message : 'Unknown error'}`);
    return false;
  }
}

interface ProvisionOptions {
  useJson: boolean;
  forceLocal: boolean;
  forceDownload: boolean;
  firmwarePath?: string;
}

async function runProvision({ useJson, forceLocal, forceDownload, firmwarePath }: ProvisionOptions): Promise<number> {
  const logger = new Logger(useJson);
  const scriptDir = path.resolve(__dirname);

  const projectRoot = (() => {
    let currentDir = scriptDir;
    while (true) {
      const buildScriptPath = path.join(currentDir, 'drum', 'build.sh');
      if (fs.existsSync(buildScriptPath)) {
        return currentDir;
      }

      const parentDir = path.dirname(currentDir);
      if (parentDir === currentDir) {
        throw new Error('Unable to locate project root (missing drum/build.sh).');
      }
      currentDir = parentDir;
    }
  })();

  const TIMEOUT_SECONDS = 30;
  const SAMPLES_DIR = 'support/samples/factory_kit';
  const BOOTLOADER_VOLUME_NAME = 'DRUMBOOT';
  const partitionJsonPath = path.join(projectRoot, 'drum', 'partition_table.json');
  const partitionUf2Candidates = [
    path.join(scriptDir, 'partition_table.uf2'),
    path.join(projectRoot, 'drum', 'build', 'partition_table.uf2'),
  ];
  let resolvedFirmwarePath: string | undefined;
  let partitionUf2Path: string | undefined;

  try {
    if (process.platform === 'win32') {
      logger.warn('Windows support is experimental. Volume detection may not work correctly.');
    }

    const requiredTools = ['picotool', 'node'];
    for (const tool of requiredTools) {
      const toolResult = spawn('which', [tool], { stdio: 'pipe' });
      await new Promise<void>((resolve, reject) => {
        toolResult.on('close', (code) => {
          if (code === 0) {
            resolve();
          } else {
            reject(new Error(`Required tool '${tool}' not found in PATH.`));
          }
        });
        toolResult.on('error', (error) => {
          reject(error);
        });
      });
    }

    process.chdir(projectRoot);

    if (firmwarePath) {
      if (!fs.existsSync(firmwarePath)) {
        throw new Error(`Specified firmware file not found: ${firmwarePath}`);
      }
      resolvedFirmwarePath = firmwarePath;
    } else if (forceLocal) {
      const localCandidates = [
        path.join(scriptDir, 'drum.uf2'),
        path.join(projectRoot, 'drum', 'build', 'drum.uf2'),
      ];
      resolvedFirmwarePath = localCandidates.find(fs.existsSync);
      if (!resolvedFirmwarePath) {
        const candidatesList = localCandidates.map((candidate) => path.relative(projectRoot, candidate));
        throw new Error(`Local firmware file not found. Expected at one of: ${candidatesList.join(', ')}`);
      }
    } else {
      const localDrumUf2 = path.join(scriptDir, 'drum.uf2');

      if (forceDownload || !fs.existsSync(localDrumUf2)) {
        const downloadedPath = await downloadLatestFirmware(scriptDir, logger);
        if (downloadedPath) {
          resolvedFirmwarePath = downloadedPath;
        }
      }

      if (!resolvedFirmwarePath) {
        const fallbackCandidates = [
          localDrumUf2,
          path.join(projectRoot, 'drum', 'build', 'drum.uf2'),
        ];
        resolvedFirmwarePath = fallbackCandidates.find(fs.existsSync);
        if (!resolvedFirmwarePath) {
          const candidatesList = fallbackCandidates.map((candidate) => path.relative(projectRoot, candidate));
          throw new Error(`Firmware file not found. Expected at one of: ${candidatesList.join(', ')}`);
        }
      }
    }

    logger.step('--- Starting Dato DRUM Provisioning ---');
    logger.warn('⚠️  WARNING: This tool will completely erase and reprogram the connected device.');
    logger.info('Starting in 3 seconds...');
    await sleep(3000);

    await waitForBootsel(TIMEOUT_SECONDS, logger);
    logger.info('✓ Device detected in BOOTSEL mode');
    await runCommand('picotool', ['info'], logger, { cwd: projectRoot, silent: true });

    logger.step('--- Step 2: White-labeling bootloader ---');
    try {
      await runCommand(
        'picotool',
        ['otp', 'white-label', '-s', '0x400', 'drum/white-label.json', '-f'],
        logger,
        { cwd: projectRoot, silent: true },
      );
      logger.info('✓ White-labeling complete');
    } catch (error) {
      if (error instanceof CommandError) {
        logger.warn('⚠ White-label programming failed (may already be programmed)');
      } else {
        throw error;
      }
    }

    logger.step('--- Step 3: Verifying white-label ---');
    await runCommand('picotool', ['reboot', '-u'], logger, { cwd: projectRoot, silent: true });
    if (process.platform === 'darwin') {
      const volumeFound = await waitForVolume(BOOTLOADER_VOLUME_NAME, TIMEOUT_SECONDS, logger);
      if (volumeFound) {
        logger.info(`✓ Volume '${BOOTLOADER_VOLUME_NAME}' verified`);
      } else {
        logger.warn('⚠ Volume verification failed (white-labeling may still be successful)');
      }
    } else {
      logger.info('✓ Volume verification skipped (non-macOS)');
    }
    await sleep(2000);

    logger.step('--- Step 4: Partitioning device ---');

    await waitForBootsel(TIMEOUT_SECONDS, logger);
    const hasPartitions = await hasRequiredPartitions(logger);

    if (hasPartitions) {
      logger.info('✓ Required partitions already exist, skipping');
    } else {
      if (!partitionUf2Path) {
        partitionUf2Path = partitionUf2Candidates.find(fs.existsSync);
        if (!partitionUf2Path && fs.existsSync(partitionJsonPath)) {
          partitionUf2Path = path.join(scriptDir, 'partition_table.uf2');
          await runCommand(
            'picotool',
            ['partition', 'create', partitionJsonPath, partitionUf2Path],
            logger,
            { cwd: projectRoot, silent: true },
          );
        }

        if (!partitionUf2Path) {
          const candidatesList = partitionUf2Candidates.map((candidate) => path.relative(projectRoot, candidate));
          throw new Error(`Partition table UF2 not found. Expected at one of: ${candidatesList.join(', ')}`);
        }
      }

      await runCommand('picotool', ['load', partitionUf2Path, '-f'], logger, { cwd: projectRoot, silent: true });
      const rebootResult = await runCommand('picotool', ['reboot', '-f', '-u'], logger, {
        cwd: projectRoot,
        ignoreFailure: true,
        silent: true,
      });
      if (rebootResult.exitCode !== 0) {
        logger.warn('⚠ Reboot failed, please reset device manually');
      }
      logger.info('✓ Partitioning complete');
    }
    await sleep(2000);

    logger.step('--- Step 5: Uploading firmware ---');
    await waitForBootsel(TIMEOUT_SECONDS, logger);
    await sleep(2000);

    if (!resolvedFirmwarePath) {
      throw new Error('Firmware path resolution failed unexpectedly.');
    }

    const firmwareDisplayPath = path.relative(projectRoot, resolvedFirmwarePath);
    logger.info(`Uploading ${firmwareDisplayPath}...`);
    await runCommand('picotool', ['load', resolvedFirmwarePath, '-f'], logger, { cwd: projectRoot, silent: true });
    await runCommand('picotool', ['reboot', '-f'], logger, { cwd: projectRoot, silent: true });
    logger.info('✓ Firmware upload complete');

    logger.step('--- Step 6: Formatting filesystem ---');
    await sleep(4000);
    await runCommand('node', ['tools/drumtool/drumtool.js', 'format'], logger, {
      cwd: projectRoot,
      input: 'y\n',
      silent: true,
    });
    logger.info('✓ Filesystem formatted');

    logger.step('--- Step 7: Uploading default samples ---');
    const samplesPath = path.join(projectRoot, SAMPLES_DIR);
    if (!fs.existsSync(samplesPath)) {
      logger.warn(`⚠ Samples directory not found, skipping`);
    } else {
      const files = fs
        .readdirSync(samplesPath, { withFileTypes: true })
        .filter((entry) => entry.isFile() && !entry.name.startsWith('.'))
        .map((entry) => path.join(SAMPLES_DIR, entry.name));

      if (files.length === 0) {
        logger.warn(`⚠ No sample files found in ${SAMPLES_DIR}`);
      } else {
        const result = await runCommand(
          'node',
          ['tools/drumtool/drumtool.js', 'send', ...files],
          logger,
          { cwd: projectRoot, ignoreFailure: true, silent: true },
        );
        if (result.exitCode === 0) {
          logger.info(`✓ Uploaded ${files.length} sample files`);
        } else {
          logger.warn('⚠ Sample upload failed');
        }
      }
    }

    logger.step('--- Step 8: Verifying installation ---');
    await runCommand('node', ['tools/drumtool/drumtool.js', 'version'], logger, { cwd: projectRoot });
    logger.info('✓ Installation verified');

    logger.step('--- Provisioning Complete! ---');

    logger.flush({ success: true });
    return 0;
  } catch (error) {
    const message = error instanceof Error ? error.message : 'Unknown error';
    logger.error(message);
    logger.flush({ success: false, error: message });
    return 1;
  }
}

async function main(): Promise<void> {
  const args = process.argv.slice(2);

  let useJson = false;
  let forceLocal = false;
  let forceDownload = false;
  let firmwarePath: string | undefined;

  for (let i = 0; i < args.length; i++) {
    const arg = args[i];

    if (arg === '--json') {
      useJson = true;
    } else if (arg === '--local') {
      forceLocal = true;
    } else if (arg === '--download') {
      forceDownload = true;
    } else if (arg === '--firmware-path') {
      if (i + 1 >= args.length) {
        console.error('Error: --firmware-path requires a value');
        process.exit(1);
      }
      firmwarePath = args[i + 1];
      i++; // Skip the next argument since we consumed it
    } else if (arg === '--help' || arg === '-h') {
      console.log(`
DRUM Provisioning Tool

Usage: node index.js [options]

Options:
  --json              Output in JSON format
  --local             Force use of local build only
  --download          Force download latest release from GitHub
  --firmware-path <path>  Specify custom firmware file path
  --help, -h          Show this help message

By default, the tool will attempt to download the latest release from GitHub,
falling back to local builds if the download fails.
`);
      process.exit(0);
    } else {
      console.error(`Error: Unknown argument: ${arg}`);
      process.exit(1);
    }
  }

  if (forceLocal && forceDownload) {
    console.error('Error: --local and --download options are mutually exclusive');
    process.exit(1);
  }

  if (firmwarePath && (forceLocal || forceDownload)) {
    console.error('Error: --firmware-path cannot be used with --local or --download');
    process.exit(1);
  }

  const exitCode = await runProvision({ useJson, forceLocal, forceDownload, firmwarePath });
  process.exit(exitCode);
}

void main();

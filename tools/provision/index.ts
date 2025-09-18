#!/usr/bin/env node

import { spawn } from 'child_process';
import * as fs from 'fs';
import * as path from 'path';

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
  logger.info('Waiting for device in BOOTSEL mode...');
  const start = Date.now();

  while (true) {
    const result = await runCommand('picotool', ['info'], logger, { silent: true, ignoreFailure: true });
    if (result.exitCode === 0) {
      logger.info('Device found in BOOTSEL mode.');
      return;
    }

    const elapsedSeconds = (Date.now() - start) / 1000;
    if (elapsedSeconds >= timeoutSeconds) {
      throw new Error(`Timed out after ${timeoutSeconds}s waiting for BOOTSEL mode.`);
    }

    await sleep(1000);
  }
}

async function waitForVolume(volumeName: string, timeoutSeconds: number, logger: Logger): Promise<void> {
  logger.info(`Waiting for volume '${volumeName}' to mount...`);
  const volumePath = path.join('/Volumes', volumeName);
  const start = Date.now();

  while (true) {
    if (fs.existsSync(volumePath)) {
      logger.info(`Volume '${volumeName}' found.`);
      return;
    }

    const elapsedSeconds = (Date.now() - start) / 1000;
    if (elapsedSeconds >= timeoutSeconds) {
      throw new Error(`Timed out after ${timeoutSeconds}s waiting for volume '${volumeName}'.`);
    }

    await sleep(1000);
  }
}

async function waitForUsbDevice(deviceName: string, timeoutSeconds: number, logger: Logger, cwd: string): Promise<void> {
  logger.info(`Waiting for USB device '${deviceName}' to appear...`);
  const start = Date.now();

  while (true) {
    const result = await runCommand(
      'system_profiler',
      ['SPUSBDataType'],
      logger,
      { silent: true, ignoreFailure: true, cwd },
    );

    if (result.exitCode === 0 && result.stdout.includes(deviceName)) {
      logger.info(`USB device '${deviceName}' found.`);
      return;
    }

    const elapsedSeconds = (Date.now() - start) / 1000;
    if (elapsedSeconds >= timeoutSeconds) {
      throw new Error(`Timed out after ${timeoutSeconds}s waiting for USB device '${deviceName}'.`);
    }

    await sleep(1000);
  }
}

interface ProvisionOptions {
  useJson: boolean;
}

async function runProvision({ useJson }: ProvisionOptions): Promise<number> {
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
  const MIDI_DEVICE_NAME = 'DRUM';
  const BOOTLOADER_VOLUME_NAME = 'DRUMBOOT';
  const partitionJsonPath = path.join(projectRoot, 'drum', 'partition_table.json');
  const firmwareCandidates = [
    path.join(scriptDir, 'drum.uf2'),
    path.join(projectRoot, 'drum', 'build', 'drum.uf2'),
  ];
  const partitionUf2Candidates = [
    path.join(scriptDir, 'partition_table.uf2'),
    path.join(projectRoot, 'drum', 'build', 'partition_table.uf2'),
  ];
  let firmwarePath: string | undefined;
  let partitionUf2Path: string | undefined;

  try {
    if (process.platform !== 'darwin') {
      throw new Error('This provisioning tool currently supports macOS only.');
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
    logger.info(`Running from project root: ${projectRoot}`);

    firmwarePath = firmwareCandidates.find(fs.existsSync);
    if (!firmwarePath) {
      const candidatesList = firmwareCandidates.map((candidate) => path.relative(projectRoot, candidate));
      throw new Error(`Firmware file not found. Expected at one of: ${candidatesList.join(', ')}`);
    }

    logger.step('--- Starting Dato DRUM Provisioning ---');
    logger.info('');
    logger.warn('⚠️  WARNING: This tool will completely erase and reprogram the connected device.');
    logger.warn('This process is destructive and irreversible.');
    logger.info('Starting automatic provisioning in 3 seconds...');
    await sleep(3000);

    await waitForBootsel(TIMEOUT_SECONDS, logger);
    await runCommand('picotool', ['info'], logger, { cwd: projectRoot });

    logger.step('--- Step 2: White-labeling bootloader ---');
    try {
      await runCommand(
        'picotool',
        ['otp', 'white-label', '-s', '0x400', 'drum/white-label.json', '-f'],
        logger,
        { cwd: projectRoot },
      );
      logger.info('White-labeling complete.');
    } catch (error) {
      if (error instanceof CommandError) {
        logger.warn('White-label programming failed. This may already be programmed.');
        logger.warn(error.result.stderr.trim() || error.message);
      } else {
        throw error;
      }
    }

    logger.step('--- Step 3: Verifying white-label ---');
    await runCommand('picotool', ['reboot', '-u'], logger, { cwd: projectRoot });
    await waitForVolume(BOOTLOADER_VOLUME_NAME, TIMEOUT_SECONDS, logger);
    logger.info(`Verified: Volume '${BOOTLOADER_VOLUME_NAME}' is present.`);
    await sleep(2000);

    logger.step('--- Step 4: Partitioning device ---');

    if (!partitionUf2Path) {
      partitionUf2Path = partitionUf2Candidates.find(fs.existsSync);
      if (!partitionUf2Path && fs.existsSync(partitionJsonPath)) {
        partitionUf2Path = path.join(scriptDir, 'partition_table.uf2');
        logger.info('Creating partition table image...');
        await runCommand(
          'picotool',
          ['partition', 'create', partitionJsonPath, partitionUf2Path],
          logger,
          { cwd: projectRoot },
        );
      }

      if (!partitionUf2Path) {
        const candidatesList = partitionUf2Candidates.map((candidate) => path.relative(projectRoot, candidate));
        throw new Error(`Partition table UF2 not found. Expected at one of: ${candidatesList.join(', ')}`);
      }
    }

    await waitForBootsel(TIMEOUT_SECONDS, logger);
    const partitionDisplayPath = path.relative(projectRoot, partitionUf2Path);
    logger.info(`Flashing partition table from ${partitionDisplayPath}...`);
    await runCommand('picotool', ['load', partitionUf2Path, '-f'], logger, { cwd: projectRoot });
    const rebootResult = await runCommand('picotool', ['reboot', '-f', '-u'], logger, {
      cwd: projectRoot,
      ignoreFailure: true,
    });
    if (rebootResult.exitCode !== 0) {
      logger.warn('Reboot command failed. Please reset the device manually.');
    }
    logger.info('Partitioning complete. Device is rebooting.');
    await sleep(2000);

    logger.step('--- Step 5: Uploading firmware ---');
    await waitForBootsel(TIMEOUT_SECONDS, logger);
    await sleep(2000);

    if (!firmwarePath) {
      throw new Error('Firmware path resolution failed unexpectedly.');
    }

    const firmwareDisplayPath = path.relative(projectRoot, firmwarePath);
    logger.info(`Uploading ${firmwareDisplayPath}...`);
    await runCommand('picotool', ['load', firmwarePath, '-f'], logger, { cwd: projectRoot });
    await runCommand('picotool', ['reboot', '-f'], logger, { cwd: projectRoot });
    logger.info('Firmware upload complete. Device is rebooting into main application.');

    logger.step('--- Step 6: Formatting filesystem ---');
    await waitForUsbDevice(MIDI_DEVICE_NAME, TIMEOUT_SECONDS, logger, projectRoot);
    await sleep(4000);
    logger.info('Device is running. Sending format command...');
    await runCommand('node', ['tools/drumtool/drumtool.js', 'format'], logger, {
      cwd: projectRoot,
      input: 'y\n',
    });

    logger.step('--- Step 7: Uploading default samples ---');
    const samplesPath = path.join(projectRoot, SAMPLES_DIR);
    if (!fs.existsSync(samplesPath)) {
      logger.warn(`Samples directory not found at ${SAMPLES_DIR}. Skipping sample upload.`);
    } else {
      const files = fs
        .readdirSync(samplesPath, { withFileTypes: true })
        .filter((entry) => entry.isFile() && !entry.name.startsWith('.'))
        .map((entry) => path.join(SAMPLES_DIR, entry.name));

      if (files.length === 0) {
        logger.warn(`No sample files found in ${SAMPLES_DIR}.`);
      } else {
        logger.info(`Uploading samples from ${SAMPLES_DIR}...`);
        const result = await runCommand(
          'node',
          ['tools/drumtool/drumtool.js', 'send', ...files],
          logger,
          { cwd: projectRoot, ignoreFailure: true },
        );
        if (result.exitCode === 0) {
          logger.info('Sample upload complete.');
        } else {
          logger.warn('Sample upload may have failed. Check device status.');
        }
      }
    }

    logger.step('--- Step 8: Verifying installation ---');
    logger.info('Checking firmware version...');
    await runCommand('node', ['tools/drumtool/drumtool.js', 'version'], logger, { cwd: projectRoot });
    logger.info('Verification step: Checking for firmware version is a basic check.');
    logger.info('A full sample verification would require extending drumtool.js.');

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
  const useJson = args.includes('--json');

  const exitCode = await runProvision({ useJson });
  process.exit(exitCode);
}

void main();

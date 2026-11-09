module.exports = function(grunt) {

  const fs = require('fs');
  const zlib = require('zlib');
  const path = require('path');
  const { execFileSync } = require('child_process');

  // Project configuration
  grunt.initConfig({
    pkg: grunt.file.readJSON('package.json'),
    
    // Clean task - removes old build files
    clean: {
      dist: ['dist'],
      data: ['data/web/**/*'],
    },
    
    // Webpack task - builds React applications
    webpack: {
      options: require('./webpack.config.js'),
      build: {
        // Configuration is loaded from webpack.config.js
      },
    },
    
    // Copy task - copies built files to data directory
    copy: {
      game: {
        files: [
          {
            expand: true,
            cwd: 'dist/game/',
            src: ['**/*'],
            dest: 'data/web/game/',
          },
        ],
      },
      system: {
        files: [
          {
            expand: true,
            cwd: 'dist/system/',
            src: ['**/*'],
            dest: 'data/web/system/',
          },
        ],
      },
      // Copy any additional assets (images, fonts, etc.)
      assets: {
        files: [
          {
            expand: true,
            cwd: 'web/game/public/',
            src: ['images/**/*', 'fonts/**/*', '*.ico', '*.png'],
            dest: 'data/web/game/',
          },
          {
            expand: true,
            cwd: 'web/system/public/',
            src: ['images/**/*', 'fonts/**/*', '*.ico', '*.png'],
            dest: 'data/web/system/',
          },
        ],
      },
      // Copy shared static assets (fonts, images, etc.) to a single LittleFS location
      shared: {
        files: [
          {
            expand: true,
            cwd: 'web/shared/',
            src: ['**/*'],
            dest: 'data/web/shared/',
          },
        ],
      },
    },
    
    // Watch task - watches for file changes and rebuilds
    watch: {
      game: {
        files: ['web/game/src/**/*', 'web/game/public/**/*'],
        tasks: ['build-game'],
        options: {
          spawn: false,
        },
      },
      system: {
        files: ['web/system/src/**/*', 'web/system/public/**/*'],
        tasks: ['build-system'],
        options: {
          spawn: false,
        },
      },
      shared: {
        files: ['web/shared/**/*'],
        tasks: ['copy:shared'],
        options: {
          spawn: false,
        },
      },
    },
  });
  
  // Custom gzip task
  grunt.registerTask('gzip', 'Gzip compress .js, .css, .html files and delete originals', function() {
    const done = this.async();
    const dataWebDir = 'data/web';
    const extensions = ['.js', '.css', '.html', '.txt', '.json'];

    // Find all files to compress
    const filesToCompress = [];

    function findFiles(dir) {
      if (!fs.existsSync(dir)) {
        grunt.log.warn('Directory does not exist: ' + dir);
        return;
      }

      const files = fs.readdirSync(dir);

      files.forEach(file => {
        const filePath = path.join(dir, file);
        const stat = fs.statSync(filePath);

        if (stat.isDirectory()) {
          findFiles(filePath);
        } else {
          const ext = path.extname(file);
          if (extensions.includes(ext)) {
            filesToCompress.push(filePath);
          }
        }
      });
    }

    // Find all files in data/web
    findFiles(dataWebDir);

    if (filesToCompress.length === 0) {
      grunt.log.warn('No files found to compress');
      done();
      return;
    }

    grunt.log.writeln('Found ' + filesToCompress.length + ' files to compress');

    let processed = 0;
    let errors = 0;

    // Process each file
    filesToCompress.forEach(filename => {
      fs.readFile(filename, 'utf8', (err, contents) => {
        if (err) {
          grunt.log.error('Error reading file: ' + filename);
          errors++;
          processed++;
          if (processed === filesToCompress.length) {
            grunt.log.writeln('Gzip compression complete: ' + (processed - errors) + ' files compressed, ' + errors + ' errors');
            done();
          }
          return;
        }

        zlib.gzip(contents, (gzipErr, zContents) => {
          if (gzipErr) {
            grunt.log.error('Error compressing file: ' + filename);
            errors++;
            processed++;
            if (processed === filesToCompress.length) {
              grunt.log.writeln('Gzip compression complete: ' + (processed - errors) + ' files compressed, ' + errors + ' errors');
              done();
            }
            return;
          }

          const gzFilename = filename + '.gz';

          fs.writeFile(gzFilename, zContents, (writeErr) => {
            if (writeErr) {
              grunt.log.error('Error writing gzip file: ' + gzFilename);
              errors++;
              processed++;
              if (processed === filesToCompress.length) {
                grunt.log.writeln('Gzip compression complete: ' + (processed - errors) + ' files compressed, ' + errors + ' errors');
                done();
              }
              return;
            }

            // Delete original file
            fs.unlink(filename, (unlinkErr) => {
              if (unlinkErr) {
                grunt.log.error('Error deleting original file: ' + filename);
                errors++;
              } else {
                grunt.log.ok('Compressed: ' + filename + ' -> ' + gzFilename);
              }

              processed++;

              if (processed === filesToCompress.length) {
                grunt.log.writeln('');
                grunt.log.writeln('Gzip compression complete: ' + (processed - errors) + ' files compressed, ' + errors + ' errors');
                done();
              }
            });
          });
        });
      });
    });
  });

  grunt.registerTask('package-release-create', 'Build binaries and create the next release directory', function() {
    const pio = process.platform === 'win32' ? 'pio.exe' : 'pio';
    const buildDir = path.join(__dirname, '.pio', 'build', 'esp8266dev');
    const releasesDir = path.join(__dirname, 'binary_releases');

    grunt.log.writeln('Building LittleFS image...');
    execFileSync(pio, ['run', '-e', 'esp8266dev', '--target', 'buildfs'], {
      cwd: __dirname,
      stdio: 'inherit',
    });

    grunt.log.writeln('Building firmware...');
    execFileSync(pio, ['run', '-e', 'esp8266dev'], {
      cwd: __dirname,
      stdio: 'inherit',
    });

    const hal = fs.readFileSync(path.join(__dirname, 'include', 'HAL.h'), 'utf8');
    const major = hal.match(/FIRMWARE_VERSION_MAJOR\s+(\d+)/);
    const minor = hal.match(/FIRMWARE_VERSION_MINOR\s+(\d+)/);
    if (!major || !minor) {
      grunt.fail.fatal('Could not read the firmware version from include/HAL.h');
    }
    const firmwareVersion = `v${major[1]}.${minor[1]}`;

    const manifest = JSON.parse(fs.readFileSync(path.join(__dirname, 'data', 'filesystem.json'), 'utf8'));
    if (!manifest.version) {
      grunt.fail.fatal('Could not read the filesystem version from data/filesystem.json');
    }
    const filesystemVersion = `v${manifest.version}`;

    const releaseNumber = fs.readdirSync(releasesDir, { withFileTypes: true })
      .filter(entry => entry.isDirectory() && /^r[0-9]+$/.test(entry.name))
      .reduce((latest, entry) => Math.max(latest, parseInt(entry.name.slice(1), 10)), 0) + 1;
    const releaseDir = path.join(releasesDir, `r${releaseNumber}`);
    fs.mkdirSync(releaseDir);

    fs.copyFileSync(path.join(buildDir, 'firmware.bin'), path.join(releaseDir, 'firmware.bin'));
    fs.copyFileSync(path.join(buildDir, 'littlefs.bin'), path.join(releaseDir, 'littlefs.bin'));

    const readmePath = path.join(releaseDir, 'README.md');
    fs.copyFileSync(path.join(releasesDir, 'README_template.md'), readmePath);
    const readme = fs.readFileSync(readmePath, 'utf8')
      .replace(/^# Release #.*$/m, `# Release #${releaseNumber}`)
      .replace('{{firmware_version}}', firmwareVersion)
      .replace('{{filesystem_version}}', filesystemVersion);
    fs.writeFileSync(readmePath, readme);

    grunt.log.ok(`Created binary_releases/r${releaseNumber}`);
    grunt.log.writeln('Edit its README.md to add hardware versions and release notes.');
  });

  // Load grunt plugins
  grunt.loadNpmTasks('grunt-contrib-clean');
  grunt.loadNpmTasks('grunt-contrib-copy');
  grunt.loadNpmTasks('grunt-contrib-watch');
  grunt.loadNpmTasks('grunt-webpack');

  // Register tasks
  grunt.registerTask('build-game', [
    'webpack:build',
    'copy:game',
    'copy:assets',
    'copy:shared',
  ]);

  grunt.registerTask('build-system', [
    'webpack:build',
    'copy:system',
    'copy:assets',
    'copy:shared',
  ]);

  grunt.registerTask('build', [
    'clean',
    'webpack:build',
    'copy:game',
    'copy:system',
    'copy:assets',
    'copy:shared',
    'gzip',
  ]);
  
  grunt.registerTask('default', ['build']);
  
  // Log completion
  grunt.registerTask('log-complete', function() {
    grunt.log.writeln('');
    grunt.log.writeln('========================================');
    grunt.log.writeln('Build completed successfully!');
    grunt.log.writeln('Game UI: data/web/game/');
    grunt.log.writeln('System UI: data/web/system/');
    grunt.log.writeln('========================================');
  });
  
  // Add log to build task
  grunt.task.renameTask('build', 'build-internal');
  grunt.registerTask('build', ['build-internal', 'log-complete']);
  grunt.registerTask('package-release', ['build', 'package-release-create']);
};


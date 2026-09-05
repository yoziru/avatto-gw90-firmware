var gulp = require('gulp');
var fs = require('fs');
var concat = require('gulp-concat');
var gzip = require('gulp-gzip');
var flatmap = require('gulp-flatmap');
var path = require('path');

function stylesConcat() {
    return gulp.src([ '../../src/websrc/css/water.css','../../src/websrc/css/style.css'])
        .pipe(concat({
            path: 'required.css',
            stat: {
                mode: 0666
            }
        }))
        .pipe(gulp.dest('../../src/websrc/css/'))
        .pipe(gzip({
            append: true
        }))
        .pipe(gulp.dest('../../src/websrc/gzipped/css/'));
}

function styles(cb) {
    var source = "../../src/websrc/gzipped/css/" + "required.css.gz";
    var destination = "../../src/webh/" + "required.css.gz.h";
 
    var wstream = fs.createWriteStream(destination);
    wstream.on('error', function (err) {
        console.log(err);
    });
 
    var data = fs.readFileSync(source);
 
    wstream.write('#define required_css_gz_len ' + data.length + '\n');
    wstream.write('const uint8_t required_css_gz[] PROGMEM = {')
 
    for (i=0; i<data.length; i++) {
        if (i % 1000 == 0) wstream.write("\n");
        wstream.write('0x' + ('00' + data[i].toString(16)).slice(-2));
        if (i<data.length-1) wstream.write(',');
    }
 
    wstream.write('\n};')
    wstream.end();
    cb();	
}


function gzDir(kind) {
	return function gz() {
		return gulp.src("../../src/websrc/" + kind + "/*.*")
			.pipe(gulp.dest("../../src/websrc/" + kind + "/"))
				.pipe(gzip({
					append: true
				}))
			.pipe(gulp.dest('../../src/websrc/gzipped/' + kind + '/'));
	};
}

function emitHeaders(kind) {
    return function emit() {
        return gulp.src("../../src/websrc/gzipped/" + kind + "/*.*")
            .pipe(flatmap(function(stream, file) {
				var filename = path.basename(file.path);
                var wstream = fs.createWriteStream("../../src/webh/" + filename + ".h");
                wstream.on("error", function(err) {
                    console.log(err);
                });
				var data = file.contents;
                wstream.write("#define " + filename.replace(/\.|-/g, "_") + "_len " + data.length + "\n");
                wstream.write("const uint8_t " + filename.replace(/\.|-/g, "_") + "[] PROGMEM = {")

                for (i = 0; i < data.length; i++) {
                    if (i % 1000 == 0) wstream.write("\n");
                    wstream.write('0x' + ('00' + data[i].toString(16)).slice(-2));
                    if (i < data.length - 1) wstream.write(',');
                }

                wstream.write("\n};")
                wstream.end();

                return stream;
            }));
    };
}

const styleTasks = gulp.series(stylesConcat, styles);
const scriptTasks = gulp.series(gzDir("js"), emitHeaders("js"));
const fontTasks = gulp.series(gzDir("fonts"), emitHeaders("fonts"));
const imgTasks = gulp.series(gzDir("img"), emitHeaders("img"));
const htmlTasks = gulp.series(gzDir("html"), emitHeaders("html"));

exports.default = gulp.parallel(styleTasks, scriptTasks, fontTasks, imgTasks, htmlTasks);
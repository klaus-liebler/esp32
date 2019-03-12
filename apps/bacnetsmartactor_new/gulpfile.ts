import * as gulp from "gulp";
import * as path from "path"
import * as browserify from "browserify";
import * as sass from "gulp-sass";
//sass.compiler = require('node-sass');
const vinylsource = require("vinyl-source-stream");
const tsify = require("tsify");
const uglify = require("gulp-uglify")
const clean = require("gulp-clean");
const inlinesource = require('gulp-inline-source');
const gzip = require('gulp-gzip');
const pug = require('gulp-pug');
const sourcemaps = require("gulp-sourcemaps");
const buffer = require("vinyl-buffer");

const wwwsrc = "wwwsrc";
const wwwdist = "wwwdist"

let globs = {
    //Important: Do not use path.join here as this leads to unexpected results (Path!=glob!)
    srchtml: [wwwsrc+"/*.html"],
    srcscss: [wwwsrc+"/index.scss"],
    srcts: [wwwsrc+"/index.ts"],
    srcpug:[wwwsrc+"/index.pug"],
    respug:[wwwdist+"/index.html"],
    bundlejs: "bundle.js",

};


function cleanDist(cb: () => void) {
    return gulp.src(wwwdist, { read: false, allowEmpty:true })
        .pipe(clean());
}


function tskTranspile(cb: () => void) {
    return browserify({
        basedir: '.',
        debug: true,
        entries: globs.srcts,
        cache: {},
        packageCache: {}
    })
        .plugin(tsify)
        .bundle()
        .pipe(vinylsource(globs.bundlejs))
        .pipe(buffer())
        .pipe(sourcemaps.init({loadmaps:true}))
        .pipe(uglify())
        .pipe(sourcemaps.write("./"))
        .pipe(gulp.dest(wwwdist));
}

function tskSass(cb: () => void) {
    return gulp.src(globs.srcscss)
        .pipe(sass({ outputStyle: 'compressed' }).on('error', sass.logError))
        .pipe(gulp.dest(wwwdist));
}

function tskPug(cb: () => void): NodeJS.ReadWriteStream {
    return gulp.src(globs.srcpug, {allowEmpty:true })
        .pipe(pug({}))
        .pipe(gulp.dest(wwwdist));
}



function tskUglify(cb: () => void) {
    // body omitted
    cb();
}

function tskCopyHTML()
{
    return gulp.src(globs.srchtml).pipe(gulp.dest(wwwdist));
}

function tskInlineAndPack() {
    return gulp.src(globs.respug)
        .pipe(inlinesource())
        .pipe(gzip())
        .pipe(gulp.dest(wwwdist));
}

if (process.env.NODE_ENV === 'production') {
    exports.default = gulp.series(cleanDist, tskTranspile, tskUglify);
} else {
    exports.default = gulp.series(cleanDist, gulp.parallel(tskTranspile, tskSass, tskCopyHTML, tskPug), tskInlineAndPack);
}
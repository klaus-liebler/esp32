var path = require('path');
var webpack = require("webpack");
var CleanWebpackPlugin = require("clean-webpack-plugin");
var MiniCssExtractPlugin = require("mini-css-extract-plugin");
var UglifyJsPlugin = require("uglifyjs-webpack-plugin");
var HtmlWebpackPlugin = require('html-webpack-plugin');
var HtmlWebpackInlineSourcePlugin = require('html-webpack-inline-source-plugin');
var OptimizeCSSAssetsPlugin = require("optimize-css-assets-webpack-plugin");
var useSourcemaps = false;
var distPath = path.join(__dirname, "wwwdist");
var srcPath = path.join(__dirname, "wwwsrc");
module.exports = {
    target: "web",
    entry: {
        main: path.join(srcPath, "index.ts")
    },
    devtool: useSourcemaps ? 'inline-source-map' : false,
    resolve: {
        extensions: ['.tsx', '.ts', '.js', '.json', '.scss']
    },
    stats: { colors: true },
    module: {
        rules: [
            {
                test: /\.tsx?$/,
                use: 'ts-loader',
                exclude: /node_modules/
            },
            {
                test: /\.scss$/,
                use: [
                    MiniCssExtractPlugin.loader,
                    "css-loader",
                    "sass-loader"
                ] //the "toolchain" for sass@webpack
            }
        ]
    },
    optimization: {
        minimizer: [
            new UglifyJsPlugin({
                cache: true,
                parallel: true,
                sourceMap: false // set to true if you want JS source maps
            }),
            new OptimizeCSSAssetsPlugin({})
        ]
    },
    plugins: [
        new MiniCssExtractPlugin({
            // Options similar to the same options in webpackOptions.output
            // both options are optional
            filename: "[name].css",
            chunkFilename: "[id].css"
        }),
        new webpack.NoEmitOnErrorsPlugin(),
        new CleanWebpackPlugin([distPath], { verbose: true }),
        new HtmlWebpackPlugin({
            inlineSource: '.(js|css)$',
            template: path.join(srcPath, "index.html"),
            minify: {
                collapseWhitespace: true,
                removeComments: true,
                removeRedundantAttributes: true,
                removeScriptTypeAttributes: true,
                removeStyleLinkTypeAttributes: true,
                useShortDoctype: true
            }
        }),
        new HtmlWebpackInlineSourcePlugin()
    ],
    output: {
        path: distPath,
        filename: "[name].js"
    }
};

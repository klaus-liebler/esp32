const path = require('path');
const webpack = require("webpack");
const CleanWebpackPlugin = require("clean-webpack-plugin");
const MiniCssExtractPlugin = require("mini-css-extract-plugin");
const UglifyJsPlugin = require("uglifyjs-webpack-plugin");
const HtmlWebpackPlugin = require('html-webpack-plugin')
const HtmlWebpackInlineSourcePlugin = require('html-webpack-inline-source-plugin');
const OptimizeCSSAssetsPlugin = require("optimize-css-assets-webpack-plugin");

let useSourcemaps = false;
let distPath=path.join(__dirname, "wwwdist");
let srcPath = path.join(__dirname, "wwwsrc");


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
                    "sass-loader"] //the "toolchain" for sass@webpack
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
            inlineSource: '.(js|css)$', // embed all javascript and css inline
            template:path.join(srcPath, "index.html"),
            minify:{
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
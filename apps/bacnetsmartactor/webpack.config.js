const webpack = require("webpack");
const path = require('path');

const CleanWebpackPlugin = require("clean-webpack-plugin");
const HtmlWebpackPlugin = require('html-webpack-plugin');

const MiniCssExtractPlugin = require("mini-css-extract-plugin");
const UglifyJsPlugin = require("uglifyjs-webpack-plugin");
const HtmlWebpackInlineSourcePlugin = require('html-webpack-inline-source-plugin');
const OptimizeCSSAssetsPlugin = require("optimize-css-assets-webpack-plugin");
const CompressionPlugin = require('compression-webpack-plugin');

let distPath = path.join(__dirname, "wwwdist");
let srcPath = path.join(__dirname, "wwwsrc");

let DEVMODE = false;

module.exports = (env, argv) => {

    if (argv.mode === 'development') {
      DEVMODE=true;
    }
    
    if (argv.mode === 'production') {
        DEVMODE=false;
    }
    console.log("============================="+argv.mode);
    let v =  {
        target: "web",
        entry: path.join(srcPath, "index.ts"),
        devtool: DEVMODE ? 'inline-source-map' : false,
        resolve: {
            extensions: ['.tsx', '.ts', '.js', '.json', '.scss']
        },
        stats: { colors: true },
        optimization: {
            minimizer: [
              new UglifyJsPlugin({
                cache: true,
                parallel: true,
                sourceMap: DEVMODE
              }),
              new OptimizeCSSAssetsPlugin({})
            ]
          },
        module: {
            rules: [
                {
                    test: /\.tsx?$/,
                    use: 'ts-loader',
                    exclude: /node_modules/
                },
                {
                    test: /\.(png|jpe?g|gif|svg|woff|woff2|ttf|eot|ico)$/,
                    loader: 'file-loader?name=assets/[name].[ext]'
                },
                {
                    test: /\.(sa|sc|c)ss$/,
                    use: [
                        DEVMODE ? 'style-loader' : MiniCssExtractPlugin.loader,
                      'css-loader',
                      'sass-loader',
                    ],
                }
            ]
        },
        plugins: [
            //new CompressionPlugin(),
            new MiniCssExtractPlugin({
                filename: "[name].css",
                chunkFilename: "[id].css"
            }),
            new CleanWebpackPlugin([distPath], { verbose: false }),
            
            new HtmlWebpackPlugin({
                inlineSource: '.(js|css)$', // embed all javascript and css inline
                template: path.join(srcPath, "index.html"),
                minify: {
                    collapseWhitespace: true,
                    removeComments: true,
                    removeRedundantAttributes: true,
                    removeScriptTypeAttributes: true,
                    removeStyleLinkTypeAttributes: true,
                    useShortDoctype: true
                }
            })
            ,new HtmlWebpackInlineSourcePlugin()
        ],
        output: {
            path: distPath,
            filename: "[name].js"
        }
    };
    console.log("============================v.devtool="+v.devtool);
    return v;
  };
const path = require("path");

module.exports = {
  entry: ["./src/index.js"],
  output: {
    path: path.resolve(__dirname, "dist"),
    filename: "bundle.js"
  },
  module: {
    rules: [
      {
        test: [/\.js$/, /\.jsx$/],
        exclude: /node_modules/,
        use: ['babel-loader']
      },
      {
        test: /\.css$/,
        use: [ 'style-loader', 'css-loader' ]
      },
      { test: /\.png$/, loader: "url-loader?mimetype=image/png" }
    ]
  },
  devServer: {
    port: 8080,
    // proxy: {
    //   '/': 'http://localhost:8000'
    // },
    watchContentBase: true,
    watchOptions: {
      ignored: [
        path.resolve(__dirname, 'dist'),
        path.resolve(__dirname, 'node_modules')
      ]
    }
    // ignored: /node_modules/
  }
  // wathcOptions: {
  //   //不监听的文件或者文件夹，支持正则匹配
  //   //默认为空
  //   ignored: /node_modules/,
  //   //监听到变化发生后会等300ms再去执行动作，防止文件更新太快
  //   //默认为300ms
  //   aggregateTimeout: 300,
  //   //判断文件是否发生变化是通过不停询问系统指定文件有没有变化实现的
  //   //默认每秒问1000次
  //   poll: 1000
  // }
};

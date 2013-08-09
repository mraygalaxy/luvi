#!/usr/bin/env nodejs
var port = (process.env.VCAP_APP_PORT || 8585);
var host = (process.env.VCAP_APP_HOST || '0.0.0.0');
var http = require('http');
var fs = require('fs');
var knox = require("knox");
var formidable = require('formidable');
var fs = require('fs');
var util = require('util');
var rpc = require('xmlrpc');
var url = require('url');
var path = require('path')
var downloading = null 
var converting = null

var mimeTypes = {
    "html": "text/html",
    "jpeg": "image/jpeg",
    "jpg": "image/jpeg",
    "png": "image/png",
    "js": "text/javascript",
    "css": "text/css"};

var options = { bucket : "hackathon", style : "path"};

if (process.env.VCAP_SERVICES) {
    s = JSON.parse(process.env.VCAP_SERVICES);
//    options.endpoint = s["blob-0.51"][0].credentials.host;
    options.endpoint = "9.47.174.6";
    options.port = s["blob-0.51"][0].credentials.port;
    options.key = s["blob-0.51"][0].credentials.username;
    options.secret = s["blob-0.51"][0].credentials.password;
} else {
    options.endpoint = "9.47.174.6";
    options.port = 45005;
    options.key = "17d05b9f-407f-4d23-8d3a-6fe9c6db928";
    options.secret = "85f29e10-fe73-4c51-a98c-fc5d3121ba3a";
}


var slaves = ["9.47.174.84", "9.47.174.85", "9.47.174.86", "9.47.174.106", "9.47.174.107"]
//slaves = ["localhost"]
var useslaves = slaves.length

var clients = {}
var client_port = 8000
var bucket = options.bucket + "/";
var client = knox.createClient(options);
var uri = "http://" + options.endpoint + ":" + options.port + "/" + bucket
var topuri = "http://" + host + ":" + port + "/"
var hero = "<div class='hero-unit' style='padding: 5px'>"

/*
// HEAD object
client.headFile(bucket + "key", function(err, res) {
      console.log("Headers:\n", res.headers);
});

*/

function bootstrap(res, output, vlist, refresh) {
 var boot = 
    [ 
     "<!DOCTYPE html>",
     "<html> ",
     "  <head> ",
     (downloading || converting) ? 
        (refresh ? "  <meta http-equiv='refresh' content='5'>" : "") : "",
     "    <title>Luvi Parallel Distributed Video Transcoder</title> ",
     "    <meta name='viewport' content='width=device-width, initial-scale=1.0'> ",
     "    <link href='luvi.css' rel='stylesheet'>",
     "    <link href='bootstrap/css/bootstrap.min.css' rel='stylesheet'> ",
     "  </head>",
     "  <body> ",
    ' <div class="navbar navbar-inverse navbar-fixed-top"> ',
      " <div class='navbar-inner' style='background-image: -webkit-linear-gradient(top, white, #DBD7D7)'> ",
        ' <div class="container"> ',
          ' <button type="button" class="btn btn-navbar" data-toggle="collapse" data-target=".nav-collapse"> ',
            ' <span class="icon-bar"></span> ',
            ' <span class="icon-bar"></span> ',
            ' <span class="icon-bar"></span> ',
          ' </button> ',
          ' <a class="brand" href="/">Luvi: Distributed Parallel Video Transcoder</a> ',
          ' <div class="nav-collapse collapse"> ',
            ' <ul class="nav"> ',
              ' <li><a href="/">vBlob Bucket Name: ' + options.bucket + '</a></li> ',
//              ' <li><a href="#about">About</a></li> ',
//              ' <li><a href="#contact">Contact</a></li> ',
            ' </ul> ',
          ' </div><!--/.nav-collapse --> ',
        ' </div> ',
      ' </div> ',
    ' </div> ',

    ' <div class="container"> ',
        vlist,
    ' </div> <!-- /container --> ',
    ' <div class="container"> ',
       "<div class='span12'>",
            output, 
        "</div>",
    ' </div> <!-- /container --> ',
        " <script src='http://code.jquery.com/jquery.js'></script> ",
        " <script src='bootstrap/js/bootstrap.min.js'></script> ",
        "</body> ",
        "</html>"
      ].join("\n")

    res.writeHead(200, {'content-type': 'text/html'})
    res.end(boot)
}


function list(data) {
      output = "<div class='span3'>\n"
      output += "<a class='btn btn-primary' href='/'>Refresh</a><br><br>\n"
      output += "<a class='btn btn-primary' href='/kill'>Stop</a><br><br>\n"
      output += "Number SDE Slaves to use:\n"
      output += "<form method='post' action='changeslaves'>\n"
      output += "<select name='nbslaves'>\n"

      for (var i = 1; i <= slaves.length; i++) {
        output += "<option"
        if (i == useslaves) {
            output += " selected"
        }
        output += ">"
        output += i + "</option>\n"
      }

      output += "</select><br>\n"
      output += "<button class='btn btn-primary' type='submit'>Change</button><br>\n"
      output += "</form>"
            
      output +=
            '<form action="/upload" enctype="multipart/form-data" method="post">' +
    //        '<input type="text" name="title"><br>' +
            "<input type='file' id='files' name='upload' class='hidden'><br><br>" +
            "<button class='btn btn-primary' type='submit'>Upload</button>" +
            '</form>'
      output += "</div>\n"
      output += "<div class='span8'>\n"

      keys = data.Contents;
//      console.log(util.inspect(data))
      output += "<table class='table'>\n"
      for (var i = 0; i < keys.length; i++) {
          var name = keys[i].Key;
          output += "<tr>\n"
          output += "<td><a class='btn"
          if (converting || downloading) {
            output += " disabled' disabled='disabled'"
          } else {
            output += "' href='/init?name=" + name + "'"
          }
          output += ">Init</a>&nbsp;</td> \n"
          output += "<td><a class='btn btn-success"
          if (converting || !downloading || (downloading != name)) {
            output += " disabled'"
          } else {
            output += "'  href='/convert?nbslaves=" + useslaves + "'"
          }
          output += ">Convert</a>&nbsp; \n"
          output += "<td><a class='btn btn-danger' href='/delete?name=" + name + "'>Delete</a>&nbsp;</td> \n"
          output += "<td>Video: Size: " + Math.floor(keys[i].Size / 1024 / 1024) + 
                    " (MB)&nbsp;</td><td>File: <a href='" + uri + name + "'>" + name + "</a></td>\n"
          output += "</tr>\n"
      }
    output += "</table>\n"
    output += "</div>\n"

    return output
}

server = http.createServer(function(req, res) {
  stuff = url.parse(req.url);
  method = req.method.toLowerCase()
  args = {};
  if (stuff.query) {
    args = JSON.parse('{"' + decodeURI(stuff.query.replace(/&/g, "\",\"").replace(/=/g,"\":\"")) + '"}');
  }

  if (stuff.pathname === '/changeslaves') {
    var form = new formidable.IncomingForm({ uploadDir: '/tmp/luvi' });

    form.parse(req, function(err, fields, files) {
        args = fields
        useslaves = parseInt(args.nbslaves)
        /*
        var string = useslaves + ""
        client.put("nbslaves", {
              "Content-Length": string.length ,
                "Content-Type": "application/json"
        }).end(string);
        */
          client.list(function(err, data) {
              bootstrap(res, hero + "<h4>Number of SDE Slaves changed to " +
                 useslaves + "</h4></div>", list(data), false)
          });
    })
  } else if (stuff.pathname === '/kill') {
    converting = null
    downloading = null
    for (var i = 0; i < slaves.length; i++) {
        client_ip = slaves[i]
        clients[client_ip].methodCall('kill', ['all'], function (error, value) {
            console.log("kill got: " + util.inspect(value))
        });
    }
      client.list(function(err, data) {
          bootstrap(res, hero + "<h4>Kill submitted.</h4></div>", 
                list(data), false)
      });
  } else if (stuff.pathname === '/convert') {
        converting = true
        for (var i = 0; i < parseInt(args.nbslaves); i++) {
            client_ip = slaves[i]
            clients[client_ip].methodCall('slave', [slaves[0]], function (error, value) {
                console.log("slaves got: " + util.inspect(value))
            });
        }
      client.list(function(err, data) {
          bootstrap(res, hero + "<h4>Slaves started using " + 
            args.nbslaves + " slaves.</h4></div>", list(data), false)
      });
  } else if (stuff.pathname === '/init') {
      if (args.name) {
        clients[slaves[0]].methodCall('master', [args.name, uri + args.name], function (error, value) {
        });
      downloading = args.name
      client.list(function(err, data) {
          bootstrap(res, hero + "<h4>Distribution of video " + 
            args.name + " started.</h4></div>" + list(data), false)
      });
    }
  } else if (stuff.pathname === '/delete') {
      if (args.name) {
          client.deleteFile(args.name, function(err, del_res) {
              result = "<h4>Delete " + args.name + ": "
              if (err) {
                  result += "Failed: " + str(err) + " " + del_res.statusCode + "<br>";
              } else {
                  result += "Success<br/>"
              }

              client.list(function(err, data) {
                  bootstrap(res, hero + result + "</h4></div>", list(data), false)
              });
          });
      } else {
          bootstrap(res, "<h3>ERROR: Filename require for deletion</h3>.<br/>", null, false)
      }
  } else if (stuff.pathname == '/upload') {
    // parse a file upload
    var form = new formidable.IncomingForm({ uploadDir: '/tmp/luvi' });

    form.parse(req, function(err, fields, files) {
        try {
            client.putFile(files.upload.path, files.upload.name, function(err_up, res_up){
               fs.unlink(files.upload.path, function(err_unlink) {
                if (err_unlink) throw err_unlink;
               });

             if (err_up) {
                bootstrap(res, hero + "<h3>Failed to upload file: " + err_up + "</h3></div>", null, false)
             } else {
                 client.list(function(err, data) {
                      output = "<h4>received upload: " + files.upload.name + "</h4></div><br>"
                      bootstrap(res, hero + output, list(data), false)
                 });
             }
          });
      } catch(fail) {
        bootstrap(res, hero + "<h3>Exception while uploading: " + fail + "</h3></div>", null, false)
      }
    });
  } else if (stuff.pathname == "/") {
//     client.getFile("nbslaves", function(err, slave_res) {
//        slave_res.on('data', function(chunk){
//          useslaves = parseInt(chunk.toString())
//          console.log("slaves: " + useslaves)
          client.list(function(err, data) {
            clients[slaves[0]].methodCall('status', [], function (error, value) {
                output = hero + "<h4>Slave status:</h4></div><br>" + 
                        value.result.replace(/(\r\n|\n|\r)/gm, "<br>") + "<p><br>"

                bootstrap(res, output, list(data), true)
            });
          });
 //     });
 //    });
      return;
  } else { 
      var filename = path.join(process.cwd(), stuff.pathname);
      path.exists(filename, function(exists) {
        if(!exists) {
            console.log("not exists: " + filename);
            res.writeHead(200, {'Content-Type': 'text/plain'});
            res.write('404 Not Found\n');
            res.end();
        }
        var mimeType = mimeTypes[path.extname(filename).split(".")[1]];
        res.writeHead(200, mimeType);

        var fileStream = fs.createReadStream(filename);
        fileStream.pipe(res);

      });
  }
});

console.log("Listening on " + host + ":" + port)

for (var i = 0; i < slaves.length; i++) {
    client_ip = slaves[i]
    clients[client_ip] = rpc.createClient({ host: client_ip, port: client_port, path: '/RPC2'})
}

console.log("Starting server.\n");

// Create bucket
var httpReq = client.put("", {});

httpReq.on('response', function (response) {
    /*
    var string = useslaves + ""
    client.put("nbslaves", {
          "Content-Length": string.length,
            "Content-Type": "application/json"
    }).end(string);
    */
    // 200 indicates successful bucket creation
    server.listen(port, host);
}).end();


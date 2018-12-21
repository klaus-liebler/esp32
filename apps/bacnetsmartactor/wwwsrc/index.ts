//import * as _ from "lodash";
import "./index.scss";

document.addEventListener("DOMContentLoaded", function(){

    //var foo = [1,2,3];
    //var bar = _.concat(foo, [4,5]);
    //console.debug(bar);//just a test for lodash!
    document.getElementById("test").innerHTML = "WebSocket is not connected";

    var websocket = new WebSocket('ws://'+location.hostname+'/w');
    var slider:HTMLInputElement = document.getElementById("myRange") as HTMLInputElement;
    
    slider.oninput = function () {
      websocket.send("L" + slider.value);
    }
    
    function sendMsg() {
      websocket.send('L50');
      console.log('Sent message to websocket');
    }
    
    websocket.onopen = function(evt) {
      console.log('WebSocket connection opened');
      websocket.send("It's open! Hooray!!!");
      document.getElementById("test").innerHTML = "WebSocket is connected!";
    }
    
    websocket.onmessage = function(evt) {
      var msg = evt.data;
      let value:string;
      switch(msg.charAt(0)) {
        case 'L':
          console.log(msg);
          value = msg.replace(/[^0-9\.]/g, '');
          slider.value = value;
          console.log("Led = " + value);
          break;
        default:
          document.getElementById("output").innerHTML = evt.data;
          break;
      }
    }
    
    websocket.onclose = function(evt) {
      console.log('Websocket connection closed');
      document.getElementById("test").innerHTML = "WebSocket closed";
    }
    
    websocket.onerror = function(evt) {
      console.log('Websocket error: ' + evt);
      document.getElementById("test").innerHTML = "WebSocket error????!!!1!!";
    }
  });



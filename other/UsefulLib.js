/*
	UsefulLib.js
	Author: Data Olsson / Emarioo
	Last updated: 2023-01-17

	About:
		This file contains utility functions for mostly drawing things on a canvas.
		The library is not supposed to be optimized but instead easy to use and quick
		to test some algorithms and display the results on a canvas.
		
	Todo:
		- Option to turn off onContext event
		- Is the scroll value what it should be?
		
	Finished:
		
*/

function StartLoop(loopFunc, fps) {
	window.onload = () => {
		InitInput();
		InitRendering();
		s_camera.x = window.innerWidth / 2;
		s_camera.y = window.innerHeight / 2;
		setInterval(() => {
			s_canvas.width = window.innerWidth;
			s_canvas.height = window.innerHeight;
			loopFunc(1/fps);
			ResetEvents();
		}, 1000 / fps);
	}
}

//-- Key & Mouse input

let s_keyNames = [];
let s_keys = {};
let s_mx = 0;
let s_my = 0;
let s_scroll = 0;

function InitInput(){
	document.addEventListener("keydown",(e)=>{
		if(e.repeat)
			return;
		// console.log(e.code);
		let key = e.code.toLowerCase();
		if(key.substring(0,3)=="key")
			key = key[3];
		if(s_keys[key]==undefined){
			s_keyNames.push(key);
			s_keys[key]={"down":true,"pressed":1};
		}else{
			s_keys[key].down=true;
			s_keys[key].pressed++;
		}
	});
	document.addEventListener("keyup",(e)=>{
		let key = e.code.toLowerCase();
		if(key.substring(0,3)=="key")
			key = key[3];
		
		if(s_keys[key]==undefined){
			s_keyNames.push(key);
			
			s_keys[key]={"down":false,"pressed":0};
		}else{
			s_keys[key].down=false;
		}
	});
	document.addEventListener("mousedown",(e)=>{
		s_mx=e.clientX;
		s_my=e.clientY;
		
		let key = null;
		if(e.button==0) key = "lm";
		else if(e.button==1) key = "mm";
		else if(e.button==2) key = "rm";
		
		if(key==null) return;
		
		if(s_keys[key]==undefined){
			s_keyNames.push(key);
			s_keys[key]={"down":true,"pressed":1};
		}else{
			s_keys[key].down=true;
			s_keys[key].pressed++;
		}
	});
	document.addEventListener("mouseup",(e)=>{
		s_mx=e.clientX;
		s_my=e.clientY;
		
		let key = null;
		if(e.button==0) key = "lm";
		else if(e.button==1) key = "mm";
		else if(e.button==2) key = "rm";
		
		if(key==null) return;
		
		if(s_keys[key]==undefined){
			s_keyNames.push(key);
			s_keys[key]={"down":false,"pressed":0};
		}else{
			s_keys[key].down=false;
		}
	});
	document.addEventListener("mousemove",(e)=>{
		s_mx=e.clientX;
		s_my=e.clientY;
	});
	document.addEventListener("wheel",(e)=>{
		s_scroll+=e.wheelDeltaY;
	});
}
function IsKeyDown(key) {
	key = key.toLowerCase();	
	if(s_keys[key]!=undefined){
		return s_keys[key].down;
	}
	return false;
}
function IsKeyPressed(key){
	key = key.toLowerCase();
	if(s_keys[key]!=undefined){
		return s_keys[key].pressed>0;	
	}
	return false;
}
function GetMouseX(){
	return s_mx;	
}
function GetMouseY(){
	return s_my;	
}
function GetWorldX(){
	return ScreenToWorldX(s_mx);
}
function GetWorldY(){
	return ScreenToWorldY(s_my);
}
function GetWheel(){
	return s_scroll;	
}
function ResetEvents(){
	for(let i=0;i<s_keyNames.length;i++){
		s_keys[s_keyNames[i]].pressed=0;	
	}
	s_scroll=0;
}

//-- Rendering

let s_canvas = null;
let s_ctx = null;
let s_camera = null;

class Camera {
	x=0;
	y=0;
	zoom=1;
}

function InitRendering() {
	s_canvas = document.createElement("canvas");
	document.body.appendChild(s_canvas);
	
	let style = document.createElement("style");
	style.textContent = `
		body{
			margin:0px;
			padding:0px;	
		}
		canvas{
			position: absolute; 
			margin:0px;	
			padding:0px;
		}
	`;
	document.head.appendChild(style);
	s_ctx = s_canvas.getContext("2d");
	s_camera = new Camera();
}
function Round(x){
	return Math.floor(x*10000)/10000;
}
function GetCamera() {
	return s_camera;	
}
function FillColor(color) {
	if (color.toHex != null) {
		s_ctx.fillStyle = color.toHex();
		s_ctx.strokeStyle = color.toHex();
	} else {
		s_ctx.fillStyle = "#" + color;
		s_ctx.strokeStyle = "#" + color;
	}
}
function FillRect(x,y,w,h){
	s_ctx.fillRect(WorldToScreenX(x)-0.5, WorldToScreenY(y)-0.5, WorldToScreenW(w) + 0.5, WorldToScreenH(h) + 0.5);
	
	// ctx.fillRect(round(x*camera.zoom-camera.x+canvas.width/2),round(y*camera.zoom-camera.y+canvas.height/2),
	//              round(w*camera.zoom),round(h*camera.zoom));
}
function FillText(x, y, str, h) {
	s_ctx.font = WorldToScreenH(h) + "px consolas";
	s_ctx.fillText(str, WorldToScreenX(x), WorldToScreenY(y));
}
function FillLine(x, y, x1, y1, size) {
	s_ctx.lineWidth = size*s_camera.zoom;
	// s_ctx.lineWidth = 5;
	// size * s_camera.zoom;
	s_ctx.beginPath();
	s_ctx.moveTo(WorldToScreenX(x), WorldToScreenY(y));
	s_ctx.lineTo(WorldToScreenX(x1), WorldToScreenY(y1));
	// s_ctx.moveTo(100,100);
	// s_ctx.lineTo(200, 200);
	// s_ctx.moveTo(x, y);
	// s_ctx.lineTo(x1, y1);
	s_ctx.stroke();
}
function FillCircle(x, y, size) {
	s_ctx.beginPath();
	s_ctx.arc(WorldToScreenX(x), WorldToScreenY(y), size*s_camera.zoom, 0, 2 * Math.PI);
	s_ctx.fill();
}
function ScreenWidth() {
	return s_canvas.width;
}
function ScreenHeight() {
	return s_canvas.height;
}
function DrawImage(img,x,y){
	s_ctx.drawImage(img,WorldToScreenX(x),WorldToScreenY(y));
	// ctx.drawImage(img,round(x*camera.zoom-camera.x+canvas.width/2),round(y*camera.zoom-camera.y+canvas.height/2));
}
function DrawSubImage(img,x,y,w,h,sx,sy,sw,sh){
	s_ctx.drawImage(img,sx,sy,sw,sh,WorldToScreenX(x),WorldToScreenY(y),WorldToScreenW(w),WorldToScreenH(h));
	// ctx.drawImage(img,sx,sy,sw,sh,round(x*camera.zoom-camera.x+canvas.width/2),round(y*camera.zoom-camera.y+canvas.height/2),
	//           round(w*camera.zoom),round(h*camera.zoom));
}
// function isFarApart(a,b,l){
// 	return (a.x-b.x)*(a.x-b.x)+(a.y-b.y)*(a.y-b.y)>l*l;
// }
function ScreenToWorldX(x){
	return (x - s_canvas.width / 2) / s_camera.zoom+s_camera.x;
}
function ScreenToWorldY(y){
	return (y - s_canvas.height / 2) / s_camera.zoom+s_camera.y;
}
function ScreenToWorldW(w){
	return w / s_camera.zoom;
}
function ScreenToWorldH(h){
	return h / s_camera.zoom;
}
function WorldToScreenX(x){
	return (x - s_camera.x) * s_camera.zoom+s_canvas.width/2;
}
function WorldToScreenY(y){
	return (y - s_camera.y) * s_camera.zoom+s_canvas.height/2;
}
function WorldToScreenW(w){
	return w * s_camera.zoom;
}
function WorldToScreenH(h){
	return h * s_camera.zoom;
}
// function isMouseInside(o){
// 	if(o.deleted)
// 		return;
// 	let mx = (eventHandler.mouseX()-canvas.width/2+camera.x)/camera.zoom;
// 	let my = (eventHandler.mouseY()-canvas.height/2+camera.y)/camera.zoom;
// 	if(o.x+o.w/2>mx&&o.x-o.w/2<mx){
// 		if(o.y+o.h/2>my&&o.y-o.h/2<my){
// 			return true;
// 		}
// 	}
// 	return false;
// }
var s_hex="0123456789ABCDEF";
class Color {
	constructor(r,g,b){
		this.r=Clamp(0,255,r);
		this.g=Clamp(0,255,g);
		this.b=Clamp(0,255,b);
	}
	// # should not be included
	static FromHex(hex){
		if(hex.length==3){
			return new Color(s_hex.indexOf(hex[0])*17,s_hex.indexOf(hex[1])*17,s_hex.indexOf(hex[2])*17);
		}else{
			return new Color(s_hex.indexOf(hex[0])*16+s_hex.indexOf(hex[1]),
			s_hex.indexOf(hex[2])*16+s_hex.indexOf(hex[3]),
			s_hex.indexOf(hex[4])*16+s_hex.indexOf(hex[5]));
		}	
	}
	darken(val){
		this.r-=val;
		this.g-=val;
		this.b-=val;
	}
	brighten(val){
		this.r+=val;
		this.g+=val;
		this.b+=val;
	}
	toHex(){
		let r=Math.floor(this.r);
		let g=Math.floor(this.g);
		let b=Math.floor(this.b);
		return s_hex[Math.floor(r/16)]+s_hex[r%16]+
			   s_hex[Math.floor(g/16)]+s_hex[g%16]+
			   s_hex[Math.floor(b/16)]+s_hex[b%16];
	}
	toString(){
		return "["+this.r+","+this.g+","+this.b+"]";
	}
	copy(){
		return new Color(this.r,this.g,this.b);	
	}
	lerp(color,f){
		return new Color(this.r*(1-lerp)+color.r*lerp,this.g*(1-lerp)+color.g*lerp,this.b*(1-lerp)+color.b*lerp);
	}
}
function srcToName(src){
	let slash = src.lastIndexOf("/")+1;
	let format = src.lastIndexOf(".");
	if(format==-1)
		format=src.length;
	return src.substr(slash,format-slash);
}

// Math
class Vector{
	x=0;
	y=0;
	add(v){
		return new Vector(this.x+v.x,this.y+v.y);	
	}
	sub(v){
		return new Vector(this.x+v.x,this.y+v.y);	
	}
	mul(v){
		return new Vector(this.x*v,this.y*v);	
	}
	div(v){
		return new Vector(this.x/v,this.y/v);	
	}
	length(){
		return Math.sqrt(Math.pow(this.x,2)+Math.pow(this.y,2));
	}
	length2(){
		return Math.pow(this.x,2)+Math.pow(this.y,2);
	}
	normalize(){
		let len = this.length();
		return this.div(len);
	}
}
function Min(a,b){
	if(a<b) return a;
	return b;
}
function Max(a,b){
	if(a>b) return a;
	return b;
}
function Clamp(min,max,val){
	if(val<min) return min;
	if(val>max) return max;
	return val;
}
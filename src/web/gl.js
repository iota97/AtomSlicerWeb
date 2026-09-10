Module.onRuntimeInitialized = () => {
	const stlPath = BigInt(stringToNewUTF8("ankle.stl"));
	_loadSTL(stlPath)
	_free(stlPath)

	const ptr = _getProfileNames();
	const names = UTF8ToString(Number(ptr)).split("/");
	_free(ptr);

	names.forEach(name => {
		const option = document.createElement("option");
		option.value = name;
		option.textContent = name.replaceAll("_", " ");
		document.getElementById("profile").appendChild(option);
	});
};

Module.onAbort = (msg) => {
    showError(msg);
}

const canvas = document.getElementById("canvas");
const gl = canvas.getContext("webgl", { preserveDrawingBuffer: true });

if (!gl) throw new Error("WebGL not supported");

gl.enable(gl.DEPTH_TEST);

const vertexShaderMeshSource = `
	attribute vec3 position;
	attribute vec3 normal;

	uniform mat4 viewProjection;
	uniform mat4 model;

	varying vec3 worldPosition;
	varying vec3 worldNormal;
	varying vec3 objectNormal;

	void main() {
		vec4 world = model * vec4(position, 1.0);
		worldPosition = world.xyz;
		worldNormal = normalize((model * vec4(normal, 0.0)).xyz);
		objectNormal = normalize(normal.zxy);
		gl_Position = viewProjection * world;
	}
`;

const fragmentShaderMeshSource = `
	precision mediump float;

	const float PI = 3.14159265359;

	varying vec3 worldPosition;
	varying vec3 worldNormal;
	varying vec3 objectNormal;

	uniform vec3 lightPosition;
	uniform vec3 slopeLimits;

	void main() {
		vec3 N = normalize(worldNormal);
		vec3 L = normalize(lightPosition - worldPosition);
		float diffuse = max(dot(N, L), 0.0);
		float lighting = 0.15 + diffuse * 0.85;

		vec3 normalColor = vec3(0.31, 0.275, 0.898);
		vec3 highlightColor = vec3(0.95, 0.25, 0.35);
		
		vec3 color = normalColor;
		if (slopeLimits.z == 2.0) { /* support-free */
			float angle = acos(-objectNormal.z)/PI*180.0;
			if (angle < 35.0 - slopeLimits.x) {
				color = highlightColor;
			}
		} else {
			float angle = acos(abs(objectNormal.z))/PI*180.0;
			if ((objectNormal.z >= 0.0 && angle < slopeLimits.x) || (objectNormal.z < 0.0 && angle < slopeLimits.y)) {
				color = highlightColor;
			}
		}

		gl_FragColor = vec4(color * lighting, 1.0);
	}
`;

const vertexShaderLineSource = `
	attribute vec3 position;
	attribute vec3 normal;
	attribute float point;

	uniform mat4 viewProjection;
	uniform mat4 model;
	uniform float maxPoint;

	varying vec3 worldPosition;
	varying vec3 worldNormal;
	varying float waypoint;

	void main() {
		vec4 world = model * vec4(position, 1.0);
		worldPosition = world.xyz;
		worldNormal = normalize((model * vec4(normal, 0.0)).xyz);
		gl_Position = viewProjection * world;
		if (point >= maxPoint) {
			gl_Position = vec4(0.0, 0.0, 0.0, 0.0);
		}
		waypoint = point;
	}
`;

const fragmentShaderLineSource = `
	precision mediump float;

	varying vec3 worldPosition;
	varying vec3 worldNormal;

	uniform vec3 lightPosition;

	void main() {
		vec3 N = normalize(worldNormal);
		vec3 L = normalize(lightPosition - worldPosition);
		float diffuse = max(dot(N, L), 0.0);
		float lighting = 0.15 + diffuse * 0.85;

		vec3 color = vec3(0.31, 0.275, 0.898);
		gl_FragColor = vec4(color * lighting, 1.0);
	}
`;

const vertexShaderLayerSource = `
	precision mediump float;

	attribute vec3 position;
	attribute vec3 normal;
	attribute float point;

	uniform mat4 viewProjection;
	uniform mat4 model;
	uniform float maxPoint;

	varying vec3 worldPosition;
	varying vec3 worldNormal;
	varying float waypoint;

	void main() {
		vec4 world = model * vec4(position, 1.0);
		worldPosition = world.xyz;
		worldNormal = normalize((model * vec4(normal, 0.0)).xyz);
		gl_Position = viewProjection * world;
		if (point >= maxPoint) {
			gl_Position = vec4(0.0, 0.0, 0.0, 0.0);
		}
		waypoint = point;
	}
`;

const fragmentShaderLayerSource = `
	precision mediump float;

	varying vec3 worldPosition;
	varying vec3 worldNormal;
	varying float waypoint;

	uniform vec3 lightPosition;
	uniform float maxPoint;

	void main() {
		vec3 N = normalize(worldNormal);
		vec3 L = normalize(lightPosition - worldPosition);
		float diffuse = max(dot(N, L), 0.0);
		float lighting = 0.3 + diffuse * 0.7;

		vec3 color = vec3(0.31, 0.275, 0.898) * (0.8 + 0.2*mod(waypoint + 0.1, 4.0));
		if (abs(waypoint - maxPoint+1.0) < 0.25) {
			color = vec3(0.95, 0.25, 0.35);
		}

		gl_FragColor = vec4(color * lighting, 1.0);
	}
`;

const vertexShaderAxisSource = `
    attribute vec3 position;
	attribute vec3 color;
	uniform mat4 matrix;

	varying vec3 vColor;

	void main() {
	    vColor = color;
		gl_Position = matrix * vec4(position, 1.0);
	}
`;

const fragmentShaderAxisSource = `
	precision mediump float;

	varying vec3 vColor;

	void main() {
		gl_FragColor = vec4(vColor, 1.0);
	}
`;

function createShader(type, source) {
	const shader = gl.createShader(type);
	gl.shaderSource(shader, source);
	gl.compileShader(shader);

	if (!gl.getShaderParameter(shader, gl.COMPILE_STATUS))
		throw new Error(gl.getShaderInfoLog(shader));

	return shader;
}

const programMesh = gl.createProgram();

gl.attachShader(programMesh, createShader(gl.VERTEX_SHADER, vertexShaderMeshSource));
gl.attachShader(programMesh, createShader(gl.FRAGMENT_SHADER, fragmentShaderMeshSource));

gl.linkProgram(programMesh);

if (!gl.getProgramParameter(programMesh, gl.LINK_STATUS))
	throw new Error(gl.getProgramInfoLog(programMesh));

gl.useProgram(programMesh);

const vertexBuffer = gl.createBuffer();
const normalBuffer = gl.createBuffer();

let meshVertexCount = 0;

const programLine = gl.createProgram();

gl.attachShader(programLine, createShader(gl.VERTEX_SHADER, vertexShaderLineSource));
gl.attachShader(programLine, createShader(gl.FRAGMENT_SHADER, fragmentShaderLineSource));

gl.linkProgram(programLine);

if (!gl.getProgramParameter(programLine, gl.LINK_STATUS))
	throw new Error(gl.getProgramInfoLog(programLine));

const lineBuffer = gl.createBuffer();

let lineVertexCount = 0;

const programLayer = gl.createProgram();

gl.attachShader(programLayer, createShader(gl.VERTEX_SHADER, vertexShaderLayerSource));
gl.attachShader(programLayer, createShader(gl.FRAGMENT_SHADER, fragmentShaderLayerSource));

gl.linkProgram(programLayer);

if (!gl.getProgramParameter(programLayer, gl.LINK_STATUS))
	throw new Error(gl.getProgramInfoLog(programLayer));

const layerBuffer = gl.createBuffer();

let layerVertexCount = 0;

const programAxis = gl.createProgram();

gl.attachShader(programAxis, createShader(gl.VERTEX_SHADER, vertexShaderAxisSource));
gl.attachShader(programAxis, createShader(gl.FRAGMENT_SHADER, fragmentShaderAxisSource));

gl.linkProgram(programAxis);

const axisVertices = new Float32Array([
    0,0,0,   1,0,0,
    1,0,0,   1,0,0,

    0,0,0,   0,0,1,
    0,1,0,   0,0,1,

    0,0,0,   0,1,0,
    0,0,1,   0,1,0
]);

const axisBuffer = gl.createBuffer();
gl.bindBuffer(gl.ARRAY_BUFFER, axisBuffer);
gl.bufferData(gl.ARRAY_BUFFER, axisVertices, gl.STATIC_DRAW);

if (!gl.getProgramParameter(programAxis, gl.LINK_STATUS))
	throw new Error(gl.getProgramInfoLog(programAxis));

function subtract(a, b) {
	return [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
}

function add(a, b) {
	return [a[0] + b[0], a[1] + b[1], a[2] + b[2]];
}

function sub(a, b) {
    return [a[0] - b[0], a[1] - b[1], a[2] - b[2]];
}

function scale(v, s) {
	return [v[0] * s, v[1] * s, v[2] * s];
}

function cross(a, b) {
	return [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
}

function dot(a, b) {
	return a[0] * b[0] + a[1] * b[1] + a[2] * b[2];
}

function normalize(v) {
	const length = Math.hypot(v[0], v[1], v[2]);

	if (length < 0.000001)
		return [0, 0, 0];

	return [v[0] / length, v[1] / length, v[2] / length];
}

function multiplyMatrices(a, b) {
	const result = new Float32Array(16);

	for (let column = 0; column < 4; column++) {
		for (let row = 0; row < 4; row++) {
			result[column * 4 + row] = a[row] * b[column * 4] + a[4 + row] * b[column * 4 + 1] + a[8 + row] * b[column * 4 + 2] + a[12 + row] * b[column * 4 + 3];
		}
	}

	return result;
}

function rotationX(angle) {
	const c = Math.cos(angle);
	const s = Math.sin(angle);

	return new Float32Array([
		1, 0, 0, 0,
		0, c, s, 0,
		0, -s, c, 0,
		0, 0, 0, 1
	]);
}

function rotationY(angle) {
	const c = Math.cos(angle);
	const s = Math.sin(angle);

	return new Float32Array([
		c, 0, -s, 0,
		0, 1, 0, 0,
		s, 0, c, 0,
		0, 0, 0, 1
	]);
}

function rotationZ(angle) {
	const c = Math.cos(angle);
	const s = Math.sin(angle);

	return new Float32Array([
		c, s, 0, 0,
		-s, c, 0, 0,
		0, 0, 1, 0,
		0, 0, 0, 1
	]);
}

function perspective(fov, aspect, near, far) {
	const f = 1 / Math.tan(fov * 0.5);

	return new Float32Array([
		f / aspect, 0, 0, 0,
		0, f, 0, 0,
		0, 0, (far + near) / (near - far), -1,
		0, 0, (2 * far * near) / (near - far), 0
	]);
}

function orthographic(left, right, bottom, top, near, far) {
	return new Float32Array([
		2 / (right - left), 0, 0, 0,
		0, 2 / (top - bottom), 0, 0,
		0, 0, -2 / (far - near), 0,
		-(right + left) / (right - left),
		-(top + bottom) / (top - bottom),
		-(far + near) / (far - near),
		1
	]);
}

function lookAt(eye, target, up) {
	const z = normalize(subtract(eye, target));
	const x = normalize(cross(up, z));
	const y = cross(z, x);

	return new Float32Array([
		x[0], y[0], z[0], 0,
		x[1], y[1], z[1], 0,
		x[2], y[2], z[2], 0,
		-dot(x, eye), -dot(y, eye), -dot(z, eye), 1
	]);
}

const identityMatrix = new Float32Array([
	1, 0, 0, 0,
	0, 1, 0, 0,
	0, 0, 1, 0,
	0, 0, 0, 1
]);

function computeSoupNormals(soup) {
	if (soup.length % 9 !== 0)
		throw new Error("Triangle soup must contain 9 floats per triangle");

	const normals = new Float32Array(soup.length);

	for (let i = 0; i < soup.length; i += 9) {
		const ax = soup[i + 0];
		const ay = soup[i + 1];
		const az = soup[i + 2];

		const bx = soup[i + 3];
		const by = soup[i + 4];
		const bz = soup[i + 5];

		const cx = soup[i + 6];
		const cy = soup[i + 7];
		const cz = soup[i + 8];

		const abx = bx - ax;
		const aby = by - ay;
		const abz = bz - az;

		const acx = cx - ax;
		const acy = cy - ay;
		const acz = cz - az;

		let nx = aby * acz - abz * acy;
		let ny = abz * acx - abx * acz;
		let nz = abx * acy - aby * acx;

		const length = Math.hypot(nx, ny, nz);

		if (length > 0.000001) {
			nx /= length;
			ny /= length;
			nz /= length;
		}

		for (let j = 0; j < 3; j++) {
			const offset = i + j * 3;
			normals[offset + 0] = nx;
			normals[offset + 1] = ny;
			normals[offset + 2] = nz;
		}
	}

	return normals;
}

function normalizeSoup(soup) {
	if (!(soup instanceof Float32Array))
		soup = new Float32Array(soup);

	if (soup.length % 9 !== 0)
		throw new Error("Triangle soup must contain 9 floats per triangle");

	let minX = Infinity;
	let minY = Infinity;
	let minZ = Infinity;
	let maxX = -Infinity;
	let maxY = -Infinity;
	let maxZ = -Infinity;

	for (let i = 0; i < soup.length; i += 3) {
		const x = soup[i];
		const y = soup[i + 2];
		const z = -soup[i + 1];

		soup[i + 0] = x;
		soup[i + 1] = y;
		soup[i + 2] = z;

		minX = Math.min(minX, x);
		minY = Math.min(minY, y);
		minZ = Math.min(minZ, z);
		maxX = Math.max(maxX, x);
		maxY = Math.max(maxY, y);
		maxZ = Math.max(maxZ, z);
	}

	const centerX = (minX + maxX) * 0.5;
	const centerY = (minY + maxY) * 0.5;
	const centerZ = (minZ + maxZ) * 0.5;

	const sizeX = maxX - minX;
	const sizeY = maxY - minY;
	const sizeZ = maxZ - minZ;

	const maxSize = Math.max(sizeX, sizeY, sizeZ);
	const scaleFactor = maxSize > 0 ? 1 / maxSize : 1;

	for (let i = 0; i < soup.length; i += 3) {
		soup[i + 0] = (soup[i + 0] - centerX) * scaleFactor;
		soup[i + 1] = (soup[i + 1] - centerY) * scaleFactor;
		soup[i + 2] = (soup[i + 2] - centerZ) * scaleFactor;
	}

	return soup;
}

function setMesh(soup) {
	gl.useProgram(programMesh);

	if (!(soup instanceof Float32Array))
		soup = new Float32Array(soup);

	if (soup.length % 9 !== 0)
		throw new Error("Triangle soup must contain 9 floats per triangle");

	const normals = computeSoupNormals(soup);

	gl.useProgram(programMesh);
	meshVertexCount = soup.length / 3;

	gl.bindBuffer(gl.ARRAY_BUFFER, vertexBuffer);
	gl.bufferData(gl.ARRAY_BUFFER, soup, gl.STATIC_DRAW);

	gl.bindBuffer(gl.ARRAY_BUFFER, normalBuffer);
	gl.bufferData(gl.ARRAY_BUFFER, normals, gl.STATIC_DRAW);

	gl.bindBuffer(gl.ARRAY_BUFFER, null);

	resetCamera();
}

function setToolpath(points) {
	gl.useProgram(programLine);

	const data = new Float32Array(points);

	lineVertexCount = data.length / 7;

	gl.bindBuffer(gl.ARRAY_BUFFER, lineBuffer);
	gl.bufferData(gl.ARRAY_BUFFER, data, gl.STATIC_DRAW);
	
	gl.bindBuffer(gl.ARRAY_BUFFER, null);
}

function setLayers(points) {
	gl.useProgram(programLayer);

	const data = new Float32Array(points);

	layerVertexCount = data.length / 7;

	gl.bindBuffer(gl.ARRAY_BUFFER, layerBuffer);
	gl.bufferData(gl.ARRAY_BUFFER, data, gl.STATIC_DRAW);
	
	gl.bindBuffer(gl.ARRAY_BUFFER, null);
}

let objectRotation = new Float32Array(identityMatrix);

const camera = {
	target: [0, -0.15, 0],
	distance: 1.8,
	yaw: 1.15,
	pitch: 0.5
};

const defaultCamera = {
	target: [0, -0.15, 0],
	distance: 1.8,
	yaw: 1.15,
	pitch: 0.5
};

function resetCamera() {
	objectRotation = new Float32Array(identityMatrix);
	camera.target = [...defaultCamera.target];
	camera.distance = defaultCamera.distance;
	camera.yaw = defaultCamera.yaw;
	camera.pitch = defaultCamera.pitch;
}

function getCameraPosition() {
	const cosPitch = Math.cos(camera.pitch);

	return [
		camera.target[0] + camera.distance * cosPitch * Math.sin(camera.yaw),
		camera.target[1] + camera.distance * Math.sin(camera.pitch),
		camera.target[2] + camera.distance * cosPitch * Math.cos(camera.yaw)
	];
}

let cameraAction = 0;
let lastX = 0;
let lastY = 0;

function beginCameraAction(action, x, y, pointerId) {
	cameraAction = action;
	lastX = x;
	lastY = y;

	try {
		if (canvas.setPointerCapture)
			canvas.setPointerCapture(pointerId);
	} catch {}
}

function endCameraAction(pointerId) {
	cameraAction = 0;
	try {
		if (canvas.releasePointerCapture && canvas.hasPointerCapture && canvas.hasPointerCapture(pointerId))
			canvas.releasePointerCapture(pointerId);
	} catch {}
}

function rotateObject(x, y) {
	const dx = x - lastX;
	const dy = y - lastY;

	const position = getCameraPosition();

	const forward = normalize(
		subtract(camera.target, position)
	);

	const screenRight = normalize(
		cross(forward, [0, 1, 0])
	);

	const screenUp = normalize(
		cross(screenRight, forward)
	);

	const yaw = dx * 0.007;
	const pitch = dy * 0.007;

	const yawRotation = rotationAroundAxis(
		screenUp,
		yaw
	);

	const pitchRotation = rotationAroundAxis(
		screenRight,
		pitch
	);

	const rotation = multiplyMatrices(
		yawRotation,
		pitchRotation
	);

	objectRotation = multiplyMatrices(
		rotation,
		objectRotation
	);
}

function rotationAroundAxis(axis, angle) {
	const x = axis[0];
	const y = axis[1];
	const z = axis[2];

	const c = Math.cos(angle);
	const s = Math.sin(angle);
	const t = 1 - c;

	return new Float32Array([
		t * x * x + c,
		t * x * y + s * z,
		t * x * z - s * y,
		0,

		t * x * y - s * z,
		t * y * y + c,
		t * y * z + s * x,
		0,

		t * x * z + s * y,
		t * y * z - s * x,
		t * z * z + c,
		0,

		0, 0, 0, 1
	]);
}

function panCamera(x, y) {
	const dx = x - lastX;
	const dy = y - lastY;

	const position = getCameraPosition();
	const forward = normalize(subtract(camera.target, position));
	const right = normalize(cross(forward, [0, 1, 0]));
	const up = normalize(cross(right, forward));

	const speed = camera.distance * 0.002;

	let movement = scale(right, -dx * speed);
	movement = add(movement, scale(up, dy * speed));

	camera.target = add(camera.target, movement);
}

function moveCamera(x, y) {
	if (!cameraAction)
		return;

	if (cameraAction === 1)
		rotateObject(x, y);

	if (cameraAction === 3)
		panCamera(x, y);

	lastX = x;
	lastY = y;
}

canvas.addEventListener("pointerdown", event => {
	if (event.button === 0)
		beginCameraAction(1, event.clientX, event.clientY, event.pointerId);

	if (event.button === 1 || event.button === 2)
		beginCameraAction(3, event.clientX, event.clientY, event.pointerId);

	if (event.button === 0 || event.button === 1 || event.button === 2)
		event.preventDefault();
});

canvas.addEventListener("pointermove", event => {
	moveCamera(event.clientX, event.clientY);
});

canvas.addEventListener("pointerup", event => {
	if (event.button === 0 || event.button === 1 || event.button === 2)
		endCameraAction(event.pointerId);
});

canvas.addEventListener("pointercancel", event => {
	endCameraAction(event.pointerId);
});

canvas.addEventListener("contextmenu", event => {
	event.preventDefault();
});

canvas.addEventListener("auxclick", event => {
	if (event.button === 1)
		event.preventDefault();
});

canvas.addEventListener("wheel", event => {
	event.preventDefault();

	camera.distance *= Math.exp(event.deltaY * 0.001);

	camera.distance = Math.max(0.2, Math.min(300, camera.distance));
}, { passive: false });


let pinchDistance = null;
let lastTouchCenter = null;

canvas.addEventListener("touchstart", event => {
	if (event.touches.length === 1) {
		const touch = event.touches[0];
		beginCameraAction(1, touch.clientX, touch.clientY, 0);
		pinchDistance = null;
		lastTouchCenter = null;
	}

	if (event.touches.length === 2) {
		cameraAction = 0;

		const a = event.touches[0];
		const b = event.touches[1];

		pinchDistance = Math.hypot(a.clientX - b.clientX, a.clientY - b.clientY);

		lastTouchCenter = {
			x: (a.clientX + b.clientX) * 0.5,
			y: (a.clientY + b.clientY) * 0.5
		};
	}

	event.preventDefault();
}, { passive: false });

canvas.addEventListener("touchmove", event => {
	if (event.touches.length === 1) {
		const touch = event.touches[0];
		moveCamera(touch.clientX, touch.clientY);
	}

	if (event.touches.length === 2) {
		const a = event.touches[0];
		const b = event.touches[1];

		const center = {
			x: (a.clientX + b.clientX) * 0.5,
			y: (a.clientY + b.clientY) * 0.5
		};

		if (lastTouchCenter !== null) {
			const dx = center.x - lastTouchCenter.x;
			const dy = center.y - lastTouchCenter.y;

			const position = getCameraPosition();
			const forward = normalize(subtract(camera.target, position));
			const right = normalize(cross(forward, [0, 1, 0]));
			const up = normalize(cross(right, forward));

			const speed = camera.distance * 0.002;

			camera.target = add(camera.target, add(scale(right, -dx * speed), scale(up, dy * speed)));
		}

		lastTouchCenter = center;

		const distance = Math.hypot(a.clientX - b.clientX, a.clientY - b.clientY);

		if (pinchDistance !== null && distance > 0.001) {
			camera.distance *= pinchDistance / distance;
			camera.distance = Math.max(0.2, Math.min(300, camera.distance));
			pinchDistance = distance;
		}
	}

	event.preventDefault();
}, { passive: false });

canvas.addEventListener("touchend", event => {
	if (event.touches.length === 0) {
		endCameraAction(0);
		pinchDistance = null;
		lastTouchCenter = null;
	}

	if (event.touches.length === 1) {
		const touch = event.touches[0];

		beginCameraAction(1, touch.clientX, touch.clientY, 0);

		pinchDistance = null;
		lastTouchCenter = null;
	}
});

canvas.addEventListener("touchcancel", () => {
	endCameraAction(0);
	pinchDistance = null;
	lastTouchCenter = null;
});

function resizeCanvas() {
	const dpr = window.devicePixelRatio || 1;
	const width = Math.round(canvas.clientWidth * dpr);
	const height = Math.round(canvas.clientHeight * dpr);

	if (canvas.width !== width || canvas.height !== height) {
		canvas.width = width;
		canvas.height = height;
	}

	gl.viewport(0, 0, canvas.width, canvas.height);
}

window.addEventListener("resize", resizeCanvas);

function createGizmoViewMatrix() {
    const eye = [2, 2, 2];
    const target = [0, 0, 0];
    const up = [0, 1, 0];

    const z = normalize(sub(eye, target));
    const x = normalize(cross(up, z));
    const y = cross(z, x);

    return new Float32Array([
        x[0], y[0], z[0], 0,
        x[1], y[1], z[1], 0,
        x[2], y[2], z[2], 0,
        0,    0,    -3,   1
    ]);
}

function renderGizmo(gl) {
    gl.useProgram(programAxis);

    const positionLocation = gl.getAttribLocation(programAxis, "position");
    const colorPosition = gl.getAttribLocation(programAxis, "color");
    const matrixPosition = gl.getUniformLocation(programAxis, "matrix");

    gl.bindBuffer(gl.ARRAY_BUFFER, axisBuffer);
    gl.enableVertexAttribArray(positionLocation);
    gl.vertexAttribPointer(positionLocation, 3, gl.FLOAT, false, 24, 0);
    gl.enableVertexAttribArray(colorPosition);
    gl.vertexAttribPointer(colorPosition, 3, gl.FLOAT, false, 24, 12);

	const view = lookAt(getCameraPosition(), camera.target, [0, 1, 0]);
    const rotation = new Float32Array(view);
    rotation[12] = rotation[13] = rotation[14] = 0;
    const proj = orthographic(-1, 1, -1, 1, -10, 10);
    const matrix = multiplyMatrices(multiplyMatrices(proj, rotation), objectRotation);
	matrix[12] += 0.25;

	const dpr = window.devicePixelRatio || 1;
    gl.viewport(10*dpr, 10*dpr, 120*dpr, 120*dpr);
    gl.disable(gl.DEPTH_TEST);

    gl.uniformMatrix4fv(matrixPosition, false, matrix);
    gl.drawArrays(gl.LINES, 0, 6);

    gl.enable(gl.DEPTH_TEST);
    gl.viewport(0, 0, gl.canvas.width, gl.canvas.height);
}

function render() {
	resizeCanvas();

	gl.clearColor(0.04, 0.04, 0.05, 1);
	gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);

	const aspect = canvas.width / canvas.height;
	const projection = perspective(Math.PI / 3, aspect, 0.01, 100);
	const view = lookAt(getCameraPosition(), camera.target, [0, 1, 0]);
	const viewProjection = multiplyMatrices(projection, view);

	if (document.getElementById('viewMode').value == "model") {
		gl.useProgram(programMesh);

		const viewProjectionLocation = gl.getUniformLocation(programMesh, "viewProjection");
		const modelLocation = gl.getUniformLocation(programMesh, "model");
		const lightPositionLocation = gl.getUniformLocation(programMesh, "lightPosition");
		const slopeLimitsLocation = gl.getUniformLocation(programMesh, "slopeLimits");
		const positionLocation = gl.getAttribLocation(programMesh, "position");
		const normalLocation = gl.getAttribLocation(programMesh, "normal");

		gl.bindBuffer(gl.ARRAY_BUFFER, vertexBuffer);
		gl.enableVertexAttribArray(positionLocation);
		gl.vertexAttribPointer(positionLocation, 3, gl.FLOAT, false, 0, 0);

		gl.bindBuffer(gl.ARRAY_BUFFER, normalBuffer);
		gl.enableVertexAttribArray(normalLocation);
		gl.vertexAttribPointer(normalLocation, 3, gl.FLOAT, false, 0, 0);

		gl.uniformMatrix4fv(viewProjectionLocation, false, viewProjection);
		gl.uniformMatrix4fv(modelLocation, false, objectRotation);
		gl.uniform3f(lightPositionLocation, 4, 5, 6);

		const topAngle = parseFloat(document.getElementById('topAngle').value);
		const bottomAngle = parseFloat(document.getElementById('bottomAngle').value);
		const toolOrientation = parseFloat(document.getElementById('toolOrientation').value);
		gl.uniform3f(slopeLimitsLocation, topAngle, bottomAngle, toolOrientation);

		gl.drawArrays(gl.TRIANGLES, 0, meshVertexCount);

		renderGizmo(gl);
	} else if (document.getElementById('viewMode').value == "layers") {
		gl.useProgram(programLayer);

		const layerViewProjectionLocation = gl.getUniformLocation(programLayer, "viewProjection");
		const layerModelLocation = gl.getUniformLocation(programLayer, "model");
		const layerPositionLocation = gl.getAttribLocation(programLayer, "position");
		const layerNormalLocation = gl.getAttribLocation(programLayer, "normal");
		const layerPointLocation = gl.getAttribLocation(programLayer, "point");
		const lightPositionLocation = gl.getUniformLocation(programLayer, "lightPosition");
		const layerMaxPointLocation = gl.getUniformLocation(programLayer, "maxPoint");

		const maxPoint = document.getElementById("layerSlider").value;

		gl.bindBuffer(gl.ARRAY_BUFFER, layerBuffer);
		const waypointStride = 7 * Float32Array.BYTES_PER_ELEMENT;
		gl.enableVertexAttribArray(layerPositionLocation);
		gl.vertexAttribPointer(layerPositionLocation, 3, gl.FLOAT, false, waypointStride, 0);
		gl.enableVertexAttribArray(layerNormalLocation);
		gl.vertexAttribPointer(layerNormalLocation, 3, gl.FLOAT, false, waypointStride, 3 * Float32Array.BYTES_PER_ELEMENT);
		gl.enableVertexAttribArray(layerPointLocation);
		gl.vertexAttribPointer(layerPointLocation, 1, gl.FLOAT, false, waypointStride, 6 * Float32Array.BYTES_PER_ELEMENT);

		gl.uniformMatrix4fv(layerViewProjectionLocation, false, viewProjection);
		gl.uniformMatrix4fv(layerModelLocation, false, objectRotation);
		gl.uniform3f(lightPositionLocation, 4, 5, 6);
		gl.uniform1f(layerMaxPointLocation, maxPoint);

		gl.drawArrays(gl.TRIANGLES, 0, layerVertexCount);
		renderGizmo(gl);
	} else {
		gl.useProgram(programLine);

		const lineViewProjectionLocation = gl.getUniformLocation(programLine, "viewProjection");
		const lineModelLocation = gl.getUniformLocation(programLine, "model");
		const linePositionLocation = gl.getAttribLocation(programLine, "position");
		const lineNormalLocation = gl.getAttribLocation(programLine, "normal");
		const linePointLocation = gl.getAttribLocation(programLine, "point");
		const lightPositionLocation = gl.getUniformLocation(programLine, "lightPosition");
		const lineMaxPointLocation = gl.getUniformLocation(programLine, "maxPoint");

		const maxPoint = document.getElementById("toolpathSlider").value;

		gl.bindBuffer(gl.ARRAY_BUFFER, lineBuffer);
		const waypointStride = 7 * Float32Array.BYTES_PER_ELEMENT;
		gl.enableVertexAttribArray(linePositionLocation);
		gl.vertexAttribPointer(linePositionLocation, 3, gl.FLOAT, false, waypointStride, 0);
		gl.enableVertexAttribArray(lineNormalLocation);
		gl.vertexAttribPointer(lineNormalLocation, 3, gl.FLOAT, false, waypointStride, 3 * Float32Array.BYTES_PER_ELEMENT);
		gl.enableVertexAttribArray(linePointLocation);
		gl.vertexAttribPointer(linePointLocation, 1, gl.FLOAT, false, waypointStride, 6 * Float32Array.BYTES_PER_ELEMENT);

		gl.uniformMatrix4fv(lineViewProjectionLocation, false, viewProjection);
		gl.uniformMatrix4fv(lineModelLocation, false, objectRotation);
		gl.uniform3f(lightPositionLocation, 4, 5, 6);
		gl.uniform1f(lineMaxPointLocation, maxPoint);

		gl.drawArrays(gl.TRIANGLES, 0, lineVertexCount);
		renderGizmo(gl);
	}

	requestAnimationFrame(render);
}

function getMesh() {
	const ptr = Number(_getTriangles());
	const vertexCount = 3 * Number(_getTrianglesCount());
	const soup = new Float32Array(Module.HEAPF32.buffer, ptr, vertexCount * 3);

	setMesh(normalizeSoup(soup));
}

function getToolpath() {
	const ptr = Number(_getToolpath());
	const pointCount = Number(_getToolpathCount());
	const toolpath = new Float32Array(Module.HEAPF32.buffer, ptr, pointCount * 7);

	const waypointCount = Number(_getWaypointCount());

	const slider = document.getElementById("toolpathSlider");
	const value = document.getElementById("toolpathValue");
	slider.max = waypointCount;
	slider.value = waypointCount;
	value.textContent = waypointCount;

	setToolpath(toolpath);
}

function getLayer() {
	const ptr = Number(_getLayer());
	const pointCount = Number(_getLayerVertexCount());
	const layers = new Float32Array(Module.HEAPF32.buffer, ptr, pointCount * 7);
	const layerCount = Number(_getLayerCount());	
	const slider = document.getElementById("layerSlider");
	const value = document.getElementById("layerValue");
	document.getElementById("constDirection").value = _getLayerDirection(BigInt(layerCount-1));
	slider.max = layerCount;
	slider.value = layerCount;
	value.textContent = layerCount;

	setLayers(layers);
}

function downloadScreenShot() {
	canvas.width = 1024;
	canvas.height = 1024;

	gl.viewport(0, 0, canvas.width, canvas.height);
	gl.clearColor(0.04, 0.04, 0.05, 1);
	gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT);

	const projection = orthographic(-0.5, 0.5, -0.5, 0.5, -2, 2);
	const view = lookAt([0, 0, 0], [0, -1, 0], [0, 0, -1]);
	const viewProjection = multiplyMatrices(projection, view);
	let noRotation = new Float32Array(identityMatrix)

	gl.useProgram(programLayer);

	const layerViewProjectionLocation = gl.getUniformLocation(programLayer, "viewProjection");
	const layerModelLocation = gl.getUniformLocation(programLayer, "model");
	const layerPositionLocation = gl.getAttribLocation(programLayer, "position");
	const layerNormalLocation = gl.getAttribLocation(programLayer, "normal");
	const layerPointLocation = gl.getAttribLocation(programLayer, "point");
	const lightPositionLocation = gl.getUniformLocation(programLayer, "lightPosition");
	const layerMaxPointLocation = gl.getUniformLocation(programLayer, "maxPoint");
	const maxPoint = document.getElementById("layerSlider").value;
	gl.bindBuffer(gl.ARRAY_BUFFER, layerBuffer);
	const waypointStride = 7 * Float32Array.BYTES_PER_ELEMENT;
	gl.enableVertexAttribArray(layerPositionLocation);
	gl.vertexAttribPointer(layerPositionLocation, 3, gl.FLOAT, false, waypointStride, 0);
	gl.enableVertexAttribArray(layerNormalLocation);
	gl.vertexAttribPointer(layerNormalLocation, 3, gl.FLOAT, false, waypointStride, 3 * Float32Array.BYTES_PER_ELEMENT);
	gl.enableVertexAttribArray(layerPointLocation);
	gl.vertexAttribPointer(layerPointLocation, 1, gl.FLOAT, false, waypointStride, 6 * Float32Array.BYTES_PER_ELEMENT);
	gl.uniformMatrix4fv(layerViewProjectionLocation, false, viewProjection);
	gl.uniformMatrix4fv(layerModelLocation, false, noRotation);
	gl.uniform3f(lightPositionLocation, 4, 5, 6);
	gl.uniform1f(layerMaxPointLocation, maxPoint);
	gl.drawArrays(gl.TRIANGLES, 0, layerVertexCount);

	const a = document.createElement('a');
	a.href = canvas.toDataURL("image/png");
	a.download = "reference.png";
	a.style.display = 'none';
	document.body.appendChild(a);
	a.click();
	document.body.removeChild(a);
	setTimeout(() => window.URL.revokeObjectURL(a.href), 1000);
	render();
}


render();
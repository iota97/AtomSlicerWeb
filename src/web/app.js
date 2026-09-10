function showError(message) {
    const errorBox = document.getElementById("errorBox");
    const errorMessage = document.getElementById("errorMessage");

    errorMessage.textContent = message;
    errorBox.style.display = "block";
}

window.addEventListener("error", function (event) {
	const msg = event.error?.message || event.message || String(event.error);
	showError(msg);
});

window.addEventListener("unhandledrejection", function (event) {
	const msg = event.reason?.message || String(event.reason);
	showError(msg);
});

if (!crossOriginIsolated) {
	throw new Error("The document is not cross-origin isolated, which is required for Emscripten threads.");
}

function updateToolOrientation() {
	const toolOrientation = document.getElementById('toolOrientation').value;
	const isSupportFree = toolOrientation === '2';
	const isConformalCovers = toolOrientation === '1';

	document.getElementById('numberOfCoverLabel').hidden = !isConformalCovers;
	document.getElementById('numberOfCover').hidden = !isConformalCovers;
	document.getElementById('bottomAngleLabel').hidden = isSupportFree;
	document.getElementById('bottomAngle').hidden = isSupportFree;
	document.getElementById('topAngleLabel').innerHTML = isSupportFree ? 'Maximum Tilt Angle (&deg;)' : 'Max Top Surface Tilt (&deg;)';
	document.getElementById('bottomAngle').disabled = isSupportFree;
}

const inputfile = "mesh_tmp";
const imageFilenames = new Map();

function slice() {
	imageFilenames.clear();
	const file = document.getElementById('inputstl').files[0];
	let path = "ankle.stl"
	if (file) {;
		const extension = file.name.split('.').pop();
		const filename = inputfile + extension;
		path = filename
	}

	const stlPath = BigInt(stringToNewUTF8(path));
	const nozzleWidth = parseFloat(document.getElementById('nozzleWidth').value);
	const topAngle = parseFloat(document.getElementById('topAngle').value);
	const bottomAngle = parseFloat(document.getElementById('bottomAngle').value);
	const numberOfCover = parseFloat(document.getElementById('numberOfCover').value);
	const collisionAngle = parseFloat(document.getElementById('collisionAngle').value);
	const zigzagOffset = parseInt(document.getElementById('zigzagOffset').value, 10);
	const upCollisionCheck = document.getElementById('upCollisionCheck').checked;
	const holeClosing = document.getElementById('holeClosing').checked;
	const infillType = parseInt(document.getElementById('infillType').value, 10);
	const toolOrientation = parseInt(document.getElementById('toolOrientation').value, 10);
	const layerFields = document.getElementById('layerFields').checked;

	_slice(stlPath, nozzleWidth, topAngle, bottomAngle, numberOfCover, collisionAngle, zigzagOffset, upCollisionCheck, holeClosing, infillType, toolOrientation, layerFields);
}

function saveProfile() {
	const output = "toolpath.gcode";
	const profile = document.getElementById("profile").value;
	const oPtr = BigInt(stringToNewUTF8(output));
	const pPtr = BigInt(stringToNewUTF8(profile));
	_saveProfile(oPtr, pPtr);
}

function downloadFile(file) {
	const array = FS.readFile(file, { encoding: 'binary' });
	const blob = new Blob([array], { type: 'application/octet-stream' });
	const url = window.URL.createObjectURL(blob);
	const a = document.createElement('a');
	a.href = url;
	a.download = file;
	a.style.display = 'none';
	document.body.appendChild(a);
	a.click();
	document.body.removeChild(a);
	setTimeout(() => window.URL.revokeObjectURL(url), 1000);
}

async function onNewSTL() {
	const file = document.getElementById('inputstl').files[0];
	if (!file) return;
	const buffer = await file.arrayBuffer();
	const array = new Uint8Array(buffer);
	const extension = file.name.split('.').pop();
	const filename = inputfile + extension;
	FS.writeFile(filename, array);
	const stlPath = BigInt(stringToNewUTF8(filename));
	_loadSTL(stlPath);
	_free(stlPath);
	document.getElementById("viewMode").value = "model";
	document.getElementById("toolpathSlider").closest(".sliderControl").style.display = "none";
	document.getElementById("layerSlider").closest(".sliderControl").style.display = "none";
}

document.getElementById("toolpathSlider").addEventListener("input", () => {
	document.getElementById("toolpathValue").textContent = document.getElementById("toolpathSlider").value;
});

document.getElementById("constDirection").addEventListener("change", function() {
	_setLayerDirection(BigInt(document.getElementById("layerSlider").value - 1), this.value);
});

document.getElementById("imageLayerCheckbox").addEventListener("change", function() {
	_setLayerFromImage(BigInt(document.getElementById("layerSlider").value - 1), this.checked);
	document.querySelectorAll('.layerImageClass').forEach(element => {
        element.style.display = this.checked ? "" : "none";
    });
});

document.getElementById("layerSlider").addEventListener("input", () => {
	document.getElementById("layerValue").textContent = document.getElementById("layerSlider").value;
	file = imageFilenames.get(document.getElementById("layerSlider").value - 1);
	if (!file) {
		document.getElementById('layerImage').value = "";
	} else {
		const dt = new DataTransfer();
		dt.items.add(new File([], file));
		document.getElementById('layerImage').files = dt.files;
	}

	document.getElementById("constDirection").value = _getLayerDirection(BigInt(document.getElementById("layerSlider").value - 1));
	document.getElementById("imageLayerCheckbox").checked = _getLayerFromImage(BigInt(document.getElementById("layerSlider").value - 1));

	document.querySelectorAll('.layerImageClass').forEach(element => {
        element.style.display = document.getElementById("imageLayerCheckbox").checked ? "" : "none";
    });
});

document.getElementById("viewMode").addEventListener("change", function() {
	document.getElementById("toolpathSlider").closest(".sliderControl").style.display = this.value == "gcode" ? "" : "none";
	document.getElementById("layerSlider").closest(".sliderControl").style.display = this.value == "layers" ? "" : "none";
});

function setProgress(progress) {
    const button = document.getElementById("sliceButton");

    button.disabled = true;
    button.style.setProperty("--progress", `${progress}%`);
    button.dataset.progress = `${progress}%`;
}

async function onNewImage() {
	const file = document.getElementById('layerImage').files[0];
	console.log('New file');
	if (!file) return;

	imageFilenames.set(document.getElementById("layerSlider").value - 1, file.name);
	const buffer = await file.arrayBuffer();
	const array = new Uint8Array(buffer);
	const extension = file.name.split('.').pop();
	const filename = "layer" + extension;
	FS.writeFile(filename, array);
	const path = BigInt(stringToNewUTF8(filename));
	_loadLayerImage(path, BigInt(document.getElementById("layerSlider").value - 1));
	_free(path);
}
self.addEventListener("install", (event) => {
	event.waitUntil((async () => {
		const cache = await caches.open("atomslicer-cache");
		await cache.addAll([
			"/",
			"/index.html",
			"/style.css",
			"/app.js",
			"/gl.js",
			"/files.js",
			"/manifest.json",
			"/files.data",
			"/slicer.js",
			"/slicer.wasm",
			"/favicon.ico"
		]);
		await self.skipWaiting();
	})());
});

self.addEventListener("activate", (event) => {
	event.waitUntil(self.clients.claim());
});

self.addEventListener("fetch", (event) => {
	if (event.request.method !== "GET") return;

	event.respondWith((async () => {
		const cache = await caches.open("atomslicer-cache");
		try {
			const response = await fetch(event.request);
			if (response.ok) {
				await cache.put(event.request, response.clone());
			}
			return response;
		} catch (error) {
			const cached = await cache.match(event.request);
			if (cached) return cached;
			throw error;
		}
	})());
});

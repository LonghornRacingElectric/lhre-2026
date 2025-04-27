/**
 * Sets up an SSE listener to receive page target updates from the server.
 */
const redirectionMap = {
    "index_page": "/index",
    "new_event_page": "/new_event",
    "running_event_page": "/create_event"
};

function setupPageSync(currentPage) {
    const eventSource = new EventSource('https://lhrelectric.org/webtool/page-sync-stream');
    eventSource.onmessage = function(event) {
        const targetPage = event.data;
        console.log("Received page sync target:", targetPage);
        if (currentPage !== targetPage) {
            console.log("NE: " + currentPage + " and " + targetPage);
            //If a redirection mapping is defined for the target, redirect
            if (redirectionMap && redirectionMap[targetPage]) {
                window.location.href = redirectionMap[targetPage];
            } else {
                console.warn("No redirection mapping for target page:", targetPage);
            }
        }
    };
    eventSource.onerror = function(err) {
        console.error("Page sync SSE error:", err);
    };
}

/**
 * Sends the new target page to the server via an HTTP POST request.
 * @param {string} target - The new target page alias to send (e.g., "index_page", "new_event_page", "running_event_page").
 * @returns {Promise} - A promise that resolves with the server response.
 */
function sendPageTarget(target) {
    return fetch('/webtool/update-page-target', {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ target_page: target })
    })
    .then(response => response.json())
    .then(data => {
        console.log("Page target updated:", data);
        return data;
    })
    .catch(err => {
        console.error("Error updating page target:", err);
        throw err;
    });
}


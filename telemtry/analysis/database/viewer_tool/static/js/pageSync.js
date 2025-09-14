/**
 * Sets up an SSE listener to receive page target updates from the server.
 */
const redirectionMap = {
    "index_page": "/webtool/index",
    "new_event_page": '/webtool/new_event/?day_id=' + window.day_id,
    "running_event_page": "/webtool/create_event"
};

let pageSyncSource;

function setupPageSync(currentPage) {
    //Close prev connections
    try {
        if (pageSyncSource) {pageSyncSource.close()}
    } catch (err) {
        console.warn("No existing page sync source to check against.")
    }
    const pageSyncSource = new EventSource('/webtool/page-sync-stream');

    pageSyncSource.onmessage = function(event) {
        const targetPage = event.data;
        console.log("Received page sync target:", targetPage);
        if (currentPage !== targetPage) {
            console.log("NE: " + currentPage + " and " + targetPage);
            //If a redirection mapping is defined for the target, redirect
            if (redirectionMap && redirectionMap[targetPage]) {
                pageSyncSource.close()
                window.location.href = redirectionMap[targetPage];
            } else {
                console.warn("No redirection mapping for target page:", targetPage);
            }
        }
    };
    pageSyncSource.onerror = function(err) {
        console.error("Page sync SSE error:", err);
    };

    window.addEventListener("beforeunload", () => pageSyncSource.close());
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


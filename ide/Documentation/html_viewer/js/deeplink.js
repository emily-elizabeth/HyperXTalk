// deeplink.js — Hash-based deep linking for the HyperXTalk dictionary.
//
// URL format:
//   api.html#entry-name
//   api.html#library/entry-name
//   api.html#library/entry-name/type
//
// When viewed in a regular browser the hash updates as you navigate entries,
// so any entry URL can be copied and shared directly.
// When running inside the LiveCode browser widget the hash logic is skipped
// and the existing template placeholder mechanism (tLibraryName / tEntryName /
// tEntryType) continues to work as before.

function deeplink_hashToArgs(pHash)
{
	if (!pHash) return null;
	var parts = pHash.replace(/^#/, '').split('/').map(decodeURIComponent);
	if (parts.length === 1) return { lib: '',       name: parts[0], type: '' };
	if (parts.length === 2) return { lib: parts[0], name: parts[1], type: '' };
	                        return { lib: parts[0], name: parts[1], type: parts[2] };
}

function deeplink_entryToHash(pEntry)
{
	var lib  = encodeURIComponent(pEntry.library || '');
	var name = encodeURIComponent(pEntry.name    || '');
	var type = encodeURIComponent(pEntry.type    || '');
	return '#' + lib + '/' + name + (type ? '/' + type : '');
}

function deeplink_install()
{
	// Only activate in a real browser, not the LiveCode widget.
	if (typeof liveCode !== 'undefined') return;

	// Wrap displayEntryAtIndex to keep the URL hash in sync.
	var _orig = displayEntryAtIndex;
	displayEntryAtIndex = function(pIndex)
	{
		_orig(pIndex);
		var entry = tState.data[pIndex];
		if (entry) history.replaceState(null, '', deeplink_entryToHash(entry));
	};

	// Navigate when the user presses browser back/forward.
	window.addEventListener('hashchange', function()
	{
		var args = deeplink_hashToArgs(window.location.hash);
		if (args && args.name) goEntryName(args.lib, args.name, args.type);
	});
}

// Returns the entry args to use on load, or null if the hash is empty/absent.
function deeplink_loadArgs()
{
	var args = deeplink_hashToArgs(window.location.hash);
	return (args && args.name) ? args : null;
}

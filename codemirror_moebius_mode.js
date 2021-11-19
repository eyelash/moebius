CodeMirror.defineSimpleMode("moebius", {
	start: [
		{regex: /\/\/.*/, token: "comment"},
		{regex: /\/\*/, token: "comment", next: "comment"},
		{regex: /"(?:[^\\]|\\.)*?(?:"|$)/, token: "string"},
		{regex: /'(?:[^\\]|\\.)*?(?:'|$)/, token: "atom"},
		{regex: /[0-9]+/, token: "number"},
		{regex: /(?:func|let|return|if|else)\b/, token: "keyword"},
		{regex: /true|false/, token: "atom"},
		{regex: /[a-zA-Z_][a-zA-Z_0-9]*/, token: "variable"}
	],
	comment: [
		{regex: /.*?\*\//, token: "comment", next: "start"},
		{regex: /.*/, token: "comment"}
	],
	meta: {
		dontIndentStates: ["comment"],
		lineComment: "//"
	}
});

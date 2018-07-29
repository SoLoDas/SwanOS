(function(args) {
	if(args.length >= 2) {
		var contents = os.read(shell.resolve(args[0]))
		if(contents) {
			var destination = shell.resolve(args[1])
			if(os.exists(destination) === "directory") destination += "/" + args[0].split("/")[-1];
			if(!os.write(destination, contents)) os.print("Couldn't write to: '" + destination + "'");
		} else {
			os.print("Couldn't read: '" + args[0] + "'");
		}
	} else {
		os.print("Not enough arguments");
	}
})
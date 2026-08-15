const DISCORD_AUTODETECT = $BLAZIUM_DISCORD_AUTODETECT;
window.DiscordEmbed = {
	isDiscordEmbed: () => {
		if (DISCORD_AUTODETECT) {
			return ['discord.com', 'discordapp.com', 'discordsays.com', 'discordapp.net', 'discordsez.com'].some((s) => window.location.hostname.includes(s));
		}
		return true;
	},
};

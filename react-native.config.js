module.exports = {
  project: {
    ios: {
      // Point autolinking at the macos/ folder since this is a macOS-only
      // react-native-macos project with no iOS target.
      sourceDir: './macos',
    },
  },
};

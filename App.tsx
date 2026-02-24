import { StatusBar } from 'expo-status-bar';
import { SafeAreaProvider } from 'react-native-safe-area-context';
import { AppProvider } from './src/store/AppContext';
import AppNavigator from './src/navigation/AppNavigator';
import { useTheme } from './src/hooks/useTheme';

function ThemedApp() {
  const theme = useTheme();
  return (
    <>
      <StatusBar style={theme.dark ? 'light' : 'dark'} />
      <AppNavigator />
    </>
  );
}

export default function App() {
  return (
    <SafeAreaProvider>
      <AppProvider>
        <ThemedApp />
      </AppProvider>
    </SafeAreaProvider>
  );
}

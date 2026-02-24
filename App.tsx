import { StatusBar } from 'expo-status-bar';
import { SafeAreaProvider } from 'react-native-safe-area-context';
import { AppProvider } from './src/store/AppContext';
import { PomodoroProvider } from './src/store/PomodoroContext';
import AppNavigator from './src/navigation/AppNavigator';
import PomodoroTimer from './src/components/PomodoroTimer';
import PomodoroMiniView from './src/components/PomodoroMiniView';
import { useTheme } from './src/hooks/useTheme';

function ThemedApp() {
  const theme = useTheme();
  return (
    <>
      <StatusBar style={theme.dark ? 'light' : 'dark'} />
      <AppNavigator />
      <PomodoroTimer />
      <PomodoroMiniView />
    </>
  );
}

export default function App() {
  return (
    <SafeAreaProvider>
      <AppProvider>
        <PomodoroProvider>
          <ThemedApp />
        </PomodoroProvider>
      </AppProvider>
    </SafeAreaProvider>
  );
}

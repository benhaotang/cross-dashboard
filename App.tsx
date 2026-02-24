import { StatusBar } from 'expo-status-bar';
import { SafeAreaProvider, SafeAreaView } from 'react-native-safe-area-context';
import { AppProvider } from './src/store/AppContext';
import { PomodoroProvider } from './src/store/PomodoroContext';
import AppNavigator from './src/navigation/AppNavigator';
import PomodoroTimer from './src/components/PomodoroTimer';
import PomodoroMiniView from './src/components/PomodoroMiniView';
import { useTheme } from './src/hooks/useTheme';

function ThemedApp() {
  const theme = useTheme();
  return (
    <SafeAreaView style={{ flex: 1, backgroundColor: theme.colors.background }} edges={['top', 'left', 'right']}>
      <StatusBar style={theme.dark ? 'light' : 'dark'} />
      <AppNavigator />
      <PomodoroTimer />
      <PomodoroMiniView />
    </SafeAreaView>
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

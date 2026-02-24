import { useColorScheme } from 'react-native';
import { lightTheme, darkTheme, Theme } from '../theme';
import { useApp } from '../store/AppContext';

export function useTheme(): Theme {
  const { state } = useApp();
  const systemScheme = useColorScheme();

  if (state.themePreference === 'dark') return darkTheme;
  if (state.themePreference === 'light') return lightTheme;
  return systemScheme === 'dark' ? darkTheme : lightTheme;
}

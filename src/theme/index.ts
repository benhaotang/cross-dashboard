export interface ThemeColors {
  primary: string;
  background: string;
  surface: string;
  border: string;
  borderLight: string;
  text: string;
  textSecondary: string;
  textTertiary: string;
  textQuaternary: string;
  filterChip: string;
  inputBackground: string;
  sidebarActiveBg: string;
}

export interface Theme {
  dark: boolean;
  colors: ThemeColors;
}

export const lightTheme: Theme = {
  dark: false,
  colors: {
    primary: '#007AFF',
    background: '#f5f5f5',
    surface: '#ffffff',
    border: '#e0e0e0',
    borderLight: '#f0f0f0',
    text: '#333333',
    textSecondary: '#666666',
    textTertiary: '#999999',
    textQuaternary: '#bbbbbb',
    filterChip: '#f0f0f0',
    inputBackground: '#fafafa',
    sidebarActiveBg: '#E3F2FD',
  },
};

export const darkTheme: Theme = {
  dark: true,
  colors: {
    primary: '#0A84FF',
    background: '#1c1c1e',
    surface: '#2c2c2e',
    border: '#3a3a3c',
    borderLight: '#2c2c2e',
    text: '#f2f2f7',
    textSecondary: '#ababab',
    textTertiary: '#636366',
    textQuaternary: '#48484a',
    filterChip: '#3a3a3c',
    inputBackground: '#1c1c1e',
    sidebarActiveBg: '#1c3050',
  },
};

export type ThemePreference = 'system' | 'light' | 'dark';

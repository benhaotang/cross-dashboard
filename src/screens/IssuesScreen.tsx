import React, { useEffect, useState } from 'react';
import {
  View,
  Text,
  StyleSheet,
  FlatList,
  TouchableOpacity,
  ActivityIndicator,
} from 'react-native';
import { useApp } from '../store/AppContext';
import { useTheme } from '../hooks/useTheme';
import * as gitea from '../services/gitea';
import * as cache from '../services/cache';
import { GiteaIssue } from '../types';
import AppIcon, { Icons } from '../components/Icon';
import IssuePropertyPage from '../components/IssuePropertyPage';

type FilterState = 'open' | 'closed' | 'all';

export default function IssuesScreen() {
  const { state, setIssues, setLoading } = useApp();
  const theme = useTheme();
  const [filter, setFilter] = useState<FilterState>('open');
  const [selectedIssue, setSelectedIssue] = useState<GiteaIssue | null>(null);

  useEffect(() => {
    if (state.giteaConfigured && state.issues.length === 0 && state.giteaRepositories.length > 0) {
      loadIssues();
    }
  }, [state.giteaConfigured, state.giteaRepositories]);

  async function loadIssues() {
    if (state.giteaRepositories.length === 0) return;

    setLoading(true);
    try {
      const issues = await gitea.fetchAllIssues(state.giteaRepositories);
      setIssues(issues);
      await cache.saveIssues(issues);
    } catch (error) {
      console.error('Error loading issues:', error);
    } finally {
      setLoading(false);
    }
  }

  function getFilteredIssues(): GiteaIssue[] {
    if (filter === 'all') return state.issues;
    return state.issues.filter((i) => i.state === filter);
  }

  async function toggleIssueState(issue: GiteaIssue) {
    const [owner, repo] = issue.repository.split('/');
    const newState = issue.state === 'open' ? 'closed' : 'open';

    const success = await gitea.updateIssueState(owner, repo, issue.number, newState);
    if (success) {
      const updated = state.issues.map((i): GiteaIssue =>
        i.id === issue.id ? { ...i, state: newState } : i
      );
      setIssues(updated);
      await cache.saveIssues(updated);
    }
  }

  const c = theme.colors;

  function renderIssue({ item }: { item: GiteaIssue }) {
    return (
      <TouchableOpacity style={[styles.issueCard, { backgroundColor: c.surface }]} onPress={() => setSelectedIssue(item)}>
        <View style={styles.issueHeader}>
          <View style={[styles.stateBadge, item.state === 'open' ? styles.stateOpen : styles.stateClosed]}>
            <Text style={styles.stateText}>{item.state}</Text>
          </View>
          <Text style={[styles.issueNumber, { color: c.textTertiary }]}>#{item.number}</Text>
        </View>

        <Text style={[styles.issueTitle, { color: c.text }]}>{item.title}</Text>
        <Text style={[styles.issueRepo, { color: c.textSecondary }]}>{item.repository}</Text>

        {item.body && (
          <Text style={[styles.issueBody, { color: c.textSecondary }]} numberOfLines={2}>
            {item.body}
          </Text>
        )}

        <View style={styles.labelContainer}>
          {item.labels.map((label) => (
            <View key={label.id} style={[styles.label, { backgroundColor: `#${label.color}` }]}>
              <Text style={styles.labelText}>{label.name}</Text>
            </View>
          ))}
        </View>

        <View style={[styles.issueFooter, { borderTopColor: c.borderLight }]}>
          <Text style={[styles.issueDate, { color: c.textTertiary }]}>
            Updated: {item.updatedAt.toLocaleDateString()}
          </Text>
          <TouchableOpacity
            style={[styles.toggleButton, { backgroundColor: c.filterChip }]}
            onPress={(e) => {
              e.stopPropagation();
              toggleIssueState(item);
            }}
          >
            <Text style={[styles.toggleText, { color: c.textSecondary }]}>
              {item.state === 'open' ? 'Close' : 'Reopen'}
            </Text>
          </TouchableOpacity>
        </View>
      </TouchableOpacity>
    );
  }

  if (!state.giteaConfigured) {
    return (
      <View style={[styles.centered, { backgroundColor: c.background }]}>
        <Text style={[styles.emptyText, { color: c.textTertiary }]}>Gitea not configured</Text>
        <Text style={[styles.hintText, { color: c.textQuaternary }]}>Go to Settings to add your Gitea instance</Text>
      </View>
    );
  }

  if (state.giteaRepositories.length === 0) {
    return (
      <View style={[styles.centered, { backgroundColor: c.background }]}>
        <Text style={[styles.emptyText, { color: c.textTertiary }]}>No repositories configured</Text>
        <Text style={[styles.hintText, { color: c.textQuaternary }]}>Add repositories in Settings</Text>
      </View>
    );
  }

  const filteredIssues = getFilteredIssues();

  return (
    <View style={[styles.container, { backgroundColor: c.background }]}>
      <View style={[styles.filterBar, { backgroundColor: c.surface, borderBottomColor: c.border }]}>
        {(['open', 'closed', 'all'] as FilterState[]).map((f) => (
          <TouchableOpacity
            key={f}
            style={[styles.filterButton, { backgroundColor: c.filterChip }, filter === f && { backgroundColor: c.primary }]}
            onPress={() => setFilter(f)}
          >
            <Text style={[styles.filterText, { color: filter === f ? '#fff' : c.textSecondary }]}>
              {f.charAt(0).toUpperCase() + f.slice(1)}
              {f !== 'all' && ` (${state.issues.filter((i) => i.state === f).length})`}
            </Text>
          </TouchableOpacity>
        ))}
        <TouchableOpacity style={styles.refreshButton} onPress={loadIssues}>
          <AppIcon name={Icons.refresh} size={16} color={c.primary} />
        </TouchableOpacity>
      </View>

      {state.isLoading ? (
        <ActivityIndicator size="large" color={c.primary} style={styles.loader} />
      ) : (
        <FlatList
          data={filteredIssues}
          renderItem={renderIssue}
          keyExtractor={(item) => item.id.toString()}
          contentContainerStyle={styles.listContent}
          ListEmptyComponent={
            <View style={styles.centered}>
              <Text style={[styles.emptyText, { color: c.textTertiary }]}>No issues found</Text>
            </View>
          }
        />
      )}

      {selectedIssue && (
        <IssuePropertyPage
          issue={selectedIssue}
          canEdit
          onClose={() => setSelectedIssue(null)}
          onSave={(updated) => {
            const newIssues = state.issues.map((i): GiteaIssue =>
              i.id === updated.id ? updated : i
            );
            setIssues(newIssues);
            cache.saveIssues(newIssues);
            setSelectedIssue(null);
          }}
          onStateToggle={async (issue) => {
            const [o, r] = issue.repository.split('/');
            const newState = issue.state === 'open' ? 'closed' : 'open';
            const success = await gitea.updateIssueState(o, r, issue.number, newState);
            if (success) {
              const updated = { ...issue, state: newState as 'open' | 'closed' };
              const newIssues = state.issues.map((i): GiteaIssue =>
                i.id === issue.id ? updated : i
              );
              setIssues(newIssues);
              cache.saveIssues(newIssues);
              setSelectedIssue(updated);
            }
          }}
        />
      )}
    </View>
  );
}

const styles = StyleSheet.create({
  container: {
    flex: 1,
  },
  centered: {
    flex: 1,
    justifyContent: 'center',
    alignItems: 'center',
    padding: 20,
  },
  filterBar: {
    flexDirection: 'row',
    padding: 12,
    borderBottomWidth: 1,
  },
  filterButton: {
    paddingHorizontal: 12,
    paddingVertical: 6,
    borderRadius: 16,
    marginRight: 8,
  },
  filterText: {
    fontSize: 14,
  },
  refreshButton: {
    marginLeft: 'auto',
    paddingHorizontal: 12,
    paddingVertical: 6,
  },
  listContent: {
    padding: 16,
  },
  issueCard: {
    padding: 16,
    borderRadius: 8,
    marginBottom: 12,
    shadowColor: '#000',
    shadowOffset: { width: 0, height: 1 },
    shadowOpacity: 0.1,
    shadowRadius: 2,
    elevation: 2,
  },
  issueHeader: {
    flexDirection: 'row',
    alignItems: 'center',
    marginBottom: 8,
  },
  stateBadge: {
    paddingHorizontal: 8,
    paddingVertical: 2,
    borderRadius: 12,
    marginRight: 8,
  },
  stateOpen: {
    backgroundColor: '#4CAF50',
  },
  stateClosed: {
    backgroundColor: '#9E9E9E',
  },
  stateText: {
    color: '#fff',
    fontSize: 12,
    fontWeight: '600',
  },
  issueNumber: {
    fontSize: 14,
  },
  issueTitle: {
    fontSize: 16,
    fontWeight: '600',
    marginBottom: 4,
  },
  issueRepo: {
    fontSize: 12,
    marginBottom: 8,
  },
  issueBody: {
    fontSize: 14,
    marginBottom: 8,
    lineHeight: 20,
  },
  labelContainer: {
    flexDirection: 'row',
    flexWrap: 'wrap',
    marginBottom: 8,
  },
  label: {
    paddingHorizontal: 8,
    paddingVertical: 2,
    borderRadius: 12,
    marginRight: 4,
    marginBottom: 4,
  },
  labelText: {
    fontSize: 10,
    color: '#fff',
    fontWeight: '600',
  },
  issueFooter: {
    flexDirection: 'row',
    justifyContent: 'space-between',
    alignItems: 'center',
    marginTop: 8,
    paddingTop: 8,
    borderTopWidth: 1,
  },
  issueDate: {
    fontSize: 12,
  },
  toggleButton: {
    paddingHorizontal: 12,
    paddingVertical: 4,
    borderRadius: 4,
  },
  toggleText: {
    fontSize: 12,
  },
  emptyText: {
    fontSize: 16,
    marginBottom: 8,
  },
  hintText: {
    fontSize: 14,
  },
  loader: {
    marginTop: 40,
  },
});

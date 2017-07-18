/***************************************************************************
 * Copyright (C) 2017  Kaloyan Raev
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ***************************************************************************/
package name.raev.kaloyan.hellostorj;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.res.AssetManager;
import android.os.Bundle;
import android.support.annotation.NonNull;
import android.support.v4.app.Fragment;
import android.support.v7.app.AppCompatActivity;
import android.support.v7.widget.RecyclerView;
import android.support.v7.widget.Toolbar;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

import name.raev.kaloyan.hellostorj.jni.Storj;

public class MainActivity extends AppCompatActivity {

    /**
     * Whether or not the activity is in two-pane mode, i.e. running on a tablet
     * device.
     */
    private boolean mTwoPane;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        Toolbar toolbar = (Toolbar) findViewById(R.id.toolbar);
        setSupportActionBar(toolbar);
        toolbar.setTitle(getTitle());

        View recyclerView = findViewById(R.id.main_list);
        assert recyclerView != null;
        setupRecyclerView((RecyclerView) recyclerView);

        if (findViewById(R.id.detail_container) != null) {
            // The detail container view will be present only in the
            // large-screen layouts (res/values-w900dp).
            // If this view is present, then the
            // activity should be in two-pane mode.
            mTwoPane = true;
        }

        copyPem();
    }

    private void setupRecyclerView(@NonNull RecyclerView recyclerView) {
        ArrayList<Integer> items = new ArrayList<>(Arrays.asList(
                R.string.title_bridge_info, R.string.title_mnemonic, R.string.title_timestamp, R.string.title_libs
        ));
        recyclerView.setAdapter(new SimpleItemRecyclerViewAdapter(items));
    }

    private void copyPem() {
        String filename = "cacert.pem";
        AssetManager assetManager = getAssets();
        try (InputStream in = assetManager.open(filename);
             OutputStream out = openFileOutput(filename, Context.MODE_PRIVATE)) {
            copy(in, out);
            Storj.caInfoPath = getFileStreamPath(filename).getPath();
        } catch(IOException e) {
            Log.e("tag", "Failed to copy asset file: " + filename, e);
        }
    }

    private void copy(InputStream in, OutputStream out) throws IOException {
        byte[] buffer = new byte[4096];
        int read;
        while((read = in.read(buffer)) != -1){
            out.write(buffer, 0, read);
        }
    }

    public class SimpleItemRecyclerViewAdapter
            extends RecyclerView.Adapter<SimpleItemRecyclerViewAdapter.ViewHolder> {

        private final List<Integer> mValues;

        public SimpleItemRecyclerViewAdapter(List<Integer> items) {
            mValues = items;
        }

        @Override
        public ViewHolder onCreateViewHolder(ViewGroup parent, int viewType) {
            View view = LayoutInflater.from(parent.getContext())
                    .inflate(android.R.layout.simple_list_item_1, parent, false);
            return new ViewHolder(view);
        }

        @Override
        public void onBindViewHolder(final ViewHolder holder, final int position) {
            holder.mText.setText(mValues.get(position));

            holder.mView.setOnClickListener(new View.OnClickListener() {
                @Override
                public void onClick(View v) {
                    if (mTwoPane) {
                        Fragment fragment = getFragment(position);
                        getSupportFragmentManager().beginTransaction()
                                .replace(R.id.detail_container, fragment)
                                .commit();
                    } else {
                        Context context = v.getContext();
                        Intent intent = new Intent(context, getActivity(position));

                        context.startActivity(intent);
                    }
                }
            });
        }

        @Override
        public int getItemCount() {
            return mValues.size();
        }

        private Class<? extends Activity> getActivity(int position) {
            switch (position) {
                case 0:
                    return BridgeInfoActivity.class;
                case 1:
                    return MnemonicActivity.class;
                case 2:
                    return TimestampActivity.class;
                case 3:
                default:
                    return LibsActivity.class;
            }
        }

        private Fragment getFragment(int position) {
            switch (position) {
                case 0:
                    return new BridgeInfoFragment();
                case 1:
                    return new MnemonicFragment();
                case 2:
                    return new TimestampFragment();
                case 3:
                default:
                    return new LibsFragment();
            }
        }

        public class ViewHolder extends RecyclerView.ViewHolder {
            public final View mView;
            public final TextView mText;

            public ViewHolder(View view) {
                super(view);
                mView = view;
                mText = (TextView) itemView.findViewById(android.R.id.text1);
            }

            @Override
            public String toString() {
                return super.toString() + " '" + mText.getText() + "'";
            }
        }
    }

}

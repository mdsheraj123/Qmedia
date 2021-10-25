/*
# Copyright (c) 2021 The Linux Foundation. All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
#       copyright notice, this list of conditions and the following
#       disclaimer in the documentation and/or other materials provided
#       with the distribution.
#     * Neither the name of The Linux Foundation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED "AS IS" AND ANY EXPRESS OR IMPLIED
# WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
# MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT
# ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
# BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
# BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
# WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
# OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
# IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

package org.codeaurora.qmedia;

import android.os.Bundle;
import android.widget.FrameLayout;

import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.FragmentTransaction;
import androidx.preference.PreferenceManager;

import com.google.android.material.tabs.TabLayout;

import org.codeaurora.qmedia.fragments.HomeFragment;
import org.codeaurora.qmedia.fragments.PermissionFragment;
import org.codeaurora.qmedia.fragments.SettingsFragment;

import java.util.Objects;

public class MainActivity extends AppCompatActivity {

    FrameLayout mFrameLayout;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        Objects.requireNonNull(getSupportActionBar()).hide();
        setContentView(R.layout.activity_main);

        mFrameLayout = findViewById(R.id.frame_layout);
        TabLayout tabLayout = findViewById(R.id.tabLayout);

        launchFragment();

        tabLayout.addOnTabSelectedListener(new TabLayout.OnTabSelectedListener() {
            @Override
            public void onTabSelected(TabLayout.Tab tab) {
                tabUpdate(tab);
            }

            @Override
            public void onTabUnselected(TabLayout.Tab tab) {

            }

            @Override
            public void onTabReselected(TabLayout.Tab tab) {
                tabUpdate(tab);
            }
        });
    }

    @Override
    protected void onResume() {
        super.onResume();
    }

    private void launchFragment() {
        PermissionFragment permissionFrag = new PermissionFragment();
        if (permissionFrag.hasPermissions(getApplicationContext())) {
            if (PreferenceManager.getDefaultSharedPreferences(this)
                    .getString("hdmi_1_source", "None").equals("None")) {
                executeTransaction(1);
            }
            executeTransaction(0);
        } else {
            executeTransaction(2);
        }
    }

    void tabUpdate(TabLayout.Tab tab) {
        executeTransaction(tab.getPosition());
    }

    public void executeTransaction(int position) {
        FragmentTransaction ft = getSupportFragmentManager().beginTransaction();
        switch (position) {
            case 0:
                ft.replace(R.id.frame_layout, new HomeFragment());
                break;
            case 1:
                ft.replace(R.id.frame_layout, new SettingsFragment());
                break;
            case 2:
                ft.replace(R.id.frame_layout, new PermissionFragment());
                break;

        }
        ft.commit();
    }
}
